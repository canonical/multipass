/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "lxd_virtual_machine.h"
#include "lxd_request.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <multipass/exceptions/local_socket_connection_exception.h>
#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_access_manager.h>
#include <multipass/snap_utils.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <shared/shared_backend_utils.h>

#include <chrono>
#include <thread>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

using namespace std::literals::chrono_literals;

namespace
{
auto instance_state_for(const QString& name, mp::NetworkAccessManager* manager, const QUrl& url)
{
    auto json_reply = lxd_request(manager, "GET", url);
    auto metadata = json_reply["metadata"].toObject();
    mpl::log(mpl::Level::trace, name.toStdString(),
             fmt::format("Got LXD container state: {} is {}", name, metadata["status"].toString()));

    switch (metadata["status_code"].toInt(-1))
    {
    case 101: // Started
    case 103: // Running
    case 107: // Stopping
    case 111: // Thawed
        return mp::VirtualMachine::State::running;
    case 102: // Stopped
        return mp::VirtualMachine::State::stopped;
    case 106: // Starting
        return mp::VirtualMachine::State::starting;
    case 109: // Freezing
        return mp::VirtualMachine::State::suspending;
    case 110: // Frozen
        return mp::VirtualMachine::State::suspended;
    case 104: // Cancelling
    case 108: // Aborting
        return mp::VirtualMachine::State::unknown;
    default:
        mpl::log(mpl::Level::error, name.toStdString(),
                 fmt::format("Got unexpected LXD state: {} ({})", metadata["status_code"].toString(),
                             metadata["status"].toInt()));
        return mp::VirtualMachine::State::unknown;
    }
}

mp::optional<mp::IPAddress> get_ip_for(const QString& mac_addr, mp::NetworkAccessManager* manager, const QUrl& url)
{
    const auto json_leases = lxd_request(manager, "GET", url);
    const auto leases = json_leases["metadata"].toArray();

    for (const auto lease : leases)
    {
        if (lease.toObject()["hwaddr"].toString() == mac_addr)
        {
            return mp::optional<mp::IPAddress>{lease.toObject()["address"].toString().toStdString()};
        }
    }

    return mp::nullopt;
}
} // namespace

mp::LXDVirtualMachine::LXDVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor,
                                         NetworkAccessManager* manager, const QUrl& base_url,
                                         const QString& bridge_name)
    : VirtualMachine{desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username},
      monitor{&monitor},
      manager{manager},
      base_url{base_url},
      bridge_name{bridge_name},
      mac_addr{QString::fromStdString(desc.default_mac_address)}
{
    try
    {
        current_state();
    }
    catch (const LXDNotFoundException& e)
    {
        mpl::log(mpl::Level::debug, name.toStdString(),
                 fmt::format("Creating instance with image id: {}", desc.image.id));

        QJsonObject config{{"limits.cpu", QString::number(desc.num_cores)},
                           {"limits.memory", QString::number(desc.mem_size.in_bytes())},
                           {"security.secureboot", "false"}};

        if (!desc.meta_data_config.IsNull())
            config["user.meta-data"] = QString::fromStdString(mpu::emit_cloud_config(desc.meta_data_config));

        if (!desc.vendor_data_config.IsNull())
            config["user.vendor-data"] = QString::fromStdString(mpu::emit_cloud_config(desc.vendor_data_config));

        if (!desc.user_data_config.IsNull())
            config["user.user-data"] = QString::fromStdString(mpu::emit_cloud_config(desc.user_data_config));

        QJsonObject devices{{"config", QJsonObject{{"source", "cloud-init:config"}, {"type", "disk"}}},
                            {"root", QJsonObject{{"path", "/"},
                                                 {"pool", "default"},
                                                 {"size", QString::number(desc.disk_space.in_bytes())},
                                                 {"type", "disk"}}},
                            {"eth0", QJsonObject{{"name", "eth0"},
                                                 {"nictype", "bridged"},
                                                 {"parent", "mpbr0"},
                                                 {"type", "nic"},
                                                 {"hwaddr", mac_addr}}}};

        QJsonObject virtual_machine{
            {"name", name},
            {"config", config},
            {"devices", devices},
            {"source", QJsonObject{{"type", "image"},
                                   {"fingerprint", QString::fromStdString(desc.image.id)}}}};

        auto json_reply = lxd_request(manager, "POST", QUrl(QString("%1/virtual-machines").arg(base_url.toString())),
                                      virtual_machine);

        lxd_wait(manager, base_url, json_reply, 300000);

        current_state();
    }
}

mp::LXDVirtualMachine::~LXDVirtualMachine()
{
    update_shutdown_status = false;

    if (state == State::running)
    {
        try
        {
            if (!QFileInfo::exists(mp::utils::snap_common_dir() + "/snap_refresh"))
                stop();
        }
        catch (const mp::SnapEnvironmentException&)
        {
            stop();
        }
    }
}

void mp::LXDVirtualMachine::start()
{
    auto present_state = current_state();

    if (present_state == State::running)
        return;

    if (present_state == State::suspending)
        throw std::runtime_error("cannot start the instance while suspending");

    if (state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Resuming from a suspended state"));
        request_state("unfreeze");
    }
    else
    {
        request_state("start");
    }

    state = State::starting;
    update_state();
}

void mp::LXDVirtualMachine::stop()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    QJsonObject stop_options;

    auto present_state = current_state();

    if (present_state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring shutdown issued while suspended"));
        return;
    }

    if (present_state == State::starting)
    {
        stop_options.insert("force", true);
    }

    request_state("stop", stop_options);

    state = State::stopped;

    if (present_state == State::starting)
    {
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
    }

    port = mp::nullopt;

    if (update_shutdown_status)
        update_state();
}

void mp::LXDVirtualMachine::shutdown()
{
    stop();
}

void mp::LXDVirtualMachine::suspend()
{
    throw std::runtime_error("suspend is currently not supported");
}

mp::VirtualMachine::State mp::LXDVirtualMachine::current_state()
{
    try
    {
        auto present_state = instance_state_for(name, manager, state_url());

        if ((state == State::delayed_shutdown || state == State::starting) && present_state == State::running)
            return state;

        state = present_state;
    }
    catch (const LocalSocketConnectionException& e)
    {
        mpl::log(mpl::Level::warning, vm_name, e.what());
        state = State::unknown;
    }

    return state;
}

int mp::LXDVirtualMachine::ssh_port()
{
    return 22;
}

void mp::LXDVirtualMachine::ensure_vm_is_running()
{
    ensure_vm_is_running(20s);
}

void mp::LXDVirtualMachine::ensure_vm_is_running(const std::chrono::milliseconds& timeout)
{
    auto is_vm_running = [this, timeout](auto& lock) {
        if (current_state() != State::stopped)
        {
            return true;
        }

        // Sleep to see if LXD is just rebooting the instance
        std::this_thread::sleep_for(timeout);

        if (current_state() != State::stopped)
        {
            state = State::starting;
            return true;
        }

        return false;
    };

    mp::backend::ensure_vm_is_running_for(this, is_vm_running, "Instance shutdown during start");
}

void mp::LXDVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

std::string mp::LXDVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    auto get_ip = [this]() -> optional<IPAddress> { return get_ip_for(mac_addr, manager, network_leases_url()); };

    return mp::backend::ip_address_for(this, get_ip, timeout);
}

std::string mp::LXDVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::LXDVirtualMachine::ipv4()
{
    if (!ip)
    {
        ip = get_ip_for(mac_addr, manager, network_leases_url());
        if (!ip)
        {
            mpl::log(mpl::Level::trace, name.toStdString(), "IP address not found.");
            return "UNKNOWN";
        }
    }

    return ip.value().as_string();
}

std::string mp::LXDVirtualMachine::ipv6()
{
    return {};
}

void mp::LXDVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mpu::wait_until_ssh_up(this, timeout, [this] { ensure_vm_is_running(); });
}

const QUrl mp::LXDVirtualMachine::url()
{
    return QString("%1/virtual-machines/%2").arg(base_url.toString()).arg(name);
}

const QUrl mp::LXDVirtualMachine::state_url()
{
    return url().toString() + "/state";
}

const QUrl mp::LXDVirtualMachine::network_leases_url()
{
    return base_url.toString() + "/networks/" + bridge_name + "/leases";
}

void mp::LXDVirtualMachine::request_state(const QString& new_state, const QJsonObject& state_options)
{
    QJsonObject state_json{{"action", new_state}};

    for (auto option = state_options.constBegin(); option != state_options.constEnd(); ++option)
    {
        state_json.insert(option.key(), option.value());
    }

    auto state_task = lxd_request(manager, "PUT", state_url(), state_json, 5000);

    try
    {
        lxd_wait(manager, base_url, state_task, 60000);
    }
    catch (const LXDNotFoundException&)
    {
        // Implies the task doesn't exist, move on...
    }
}

/*
 * Copyright (C) 2019 Canonical, Ltd.
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
#include <QtNetwork/QNetworkAccessManager>

#include <multipass/exceptions/start_exception.h>
#include <multipass/logging/log.h>
#include <multipass/ip_address.h>
#include <multipass/optional.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <fmt/format.h>

#include <QDebug>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
auto instance_state_for(const QString& name, QNetworkAccessManager* manager, const QUrl& url)
{
    auto json_reply = lxd_request(manager, "GET", url);
    auto metadata = json_reply["metadata"].toObject();
    mpl::log(mpl::Level::debug, name.toStdString(), fmt::format("Got LXD container state: {} is {}", name, metadata["status"].toString()));

    switch(metadata["status_code"].toInt(-1))
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
            mpl::log(mpl::Level::error, name.toStdString(), fmt::format("Got unexpected LXD state: {} ({})", metadata["status_code"].toString(), metadata["status"].toInt()));
            return mp::VirtualMachine::State::unknown;
    }
}

const mp::optional<mp::IPAddress> get_ip_for(const QString& name, QNetworkAccessManager* manager, const QUrl& url)
{
    const auto json_state = lxd_request(manager, "GET", url);
    const auto addresses = json_state["metadata"].toObject()["network"].toObject()["eth0"].toObject()["addresses"].toArray();

    for (const auto address : addresses)
    {
        if (address.toObject()["family"].toString() == "inet")
            return mp::optional<mp::IPAddress>{address.toObject()["address"].toString().toStdString()};
    }

    mpl::log(mpl::Level::debug, name.toStdString(), fmt::format("IP for {} not found...", name));
    return mp::nullopt;
}

} // namespace

mp::LXDVirtualMachine::LXDVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor, QNetworkAccessManager* manager, const QUrl& base_url)
    : VirtualMachine{desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username},
      monitor{&monitor},
      base_url{base_url},
      manager{manager}
{
    try
    {
        instance_state_for(name, manager, url());
    }
    catch(const LXDNotFoundException& e)
    {
        mpl::log(mpl::Level::debug, name.toStdString(), fmt::format("Creating container with stream: {}, id: {}", desc.image.stream_location, desc.image.id));

        QJsonObject config{
            {"limits.cpu", QString::number(desc.num_cores)},
            {"limits.memory", QString::number(desc.mem_size.in_bytes())}
        };

        if (!desc.meta_data_config.IsNull())
            config["user.meta-data"] = QString::fromStdString(mpu::emit_cloud_config(desc.meta_data_config));

        if (!desc.vendor_data_config.IsNull())
            config["user.vendor-data"] = QString::fromStdString(mpu::emit_cloud_config(desc.vendor_data_config));

        if (!desc.user_data_config.IsNull())
            config["user.user-data"] = QString::fromStdString(mpu::emit_cloud_config(desc.user_data_config));

        QJsonObject container{
            {"name", name},
            {"config", config},
            {"source", QJsonObject{
                {"type", "image"},
                {"mode", "pull"},
                {"server", QString::fromStdString(desc.image.stream_location)},
                {"protocol", "simplestreams"},
                {"fingerprint", QString::fromStdString(desc.image.id)}
            }}
        };

        auto json_reply = lxd_request(manager, "POST", QUrl(QString("%1/containers").arg(base_url.toString())), container);
        mpl::log(mpl::Level::debug, name.toStdString(), fmt::format("Got LXD creation reply: {}", QJsonDocument(json_reply).toJson()));

        state = instance_state_for(name, manager, url());
    }
}

mp::LXDVirtualMachine::~LXDVirtualMachine()
{
    update_suspend_status = false;

    if (current_state() == State::running)
        suspend();
}

void mp::LXDVirtualMachine::start()
{
    auto present_state = current_state();

    if (present_state == State::running)
        return;

    request_state("start");

    state = State::starting;
    update_state();
}

void mp::LXDVirtualMachine::stop()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    auto present_state = current_state();

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        state = State::stopped;
        port = mp::nullopt;
    }
    else if (present_state == State::starting)
    {
        state = State::off;
        state_wait.wait(lock, [this] { return state == State::stopped; });
        port = mp::nullopt;
    }
    else if (present_state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring shutdown issued while suspended"));
    }

    update_state();
    lock.unlock();
}

void mp::LXDVirtualMachine::shutdown()
{
    stop();
}

void mp::LXDVirtualMachine::suspend()
{
    auto present_state = instance_state_for(name, manager, base_url);

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        if (update_suspend_status)
        {
            state = State::suspended;
            update_state();
        }
    }
    else if (present_state == State::stopped)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring suspend issued while stopped"));
    }

    monitor->on_suspend();
}

mp::VirtualMachine::State mp::LXDVirtualMachine::current_state()
{
    auto present_state = instance_state_for(name, manager, url());

    if ((state == State::delayed_shutdown && present_state == State::running) || state == State::starting)
        return state;

    state = present_state;
    return state;
}

int mp::LXDVirtualMachine::ssh_port()
{
    return 22;
}

void mp::LXDVirtualMachine::ensure_vm_is_running()
{
    std::lock_guard<decltype(state_mutex)> lock{state_mutex};
    if (state == State::off)
    {
        // Have to set 'stopped' here so there is an actual state change to compare to for
        // the cond var's predicate
        state = State::stopped;
        state_wait.notify_all();
        throw mp::StartException(vm_name, "Instance shutdown during start");
    }
}

void mp::LXDVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

std::string mp::LXDVirtualMachine::ssh_hostname()
{
    if (!ip)
    {
        auto action = [this] {
            ensure_vm_is_running();
            ip = get_ip_for(name, std::make_unique<QNetworkAccessManager>().get(), state_url());
            if (ip)
            {
                return mp::utils::TimeoutAction::done;
            }
            else
            {
                return mp::utils::TimeoutAction::retry;
            }
        };
        auto on_timeout = [] { return std::runtime_error("failed to determine IP address"); };
        mp::utils::try_action_for(on_timeout, std::chrono::minutes(2), action);
    }

    return ip.value().as_string();
}

std::string mp::LXDVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::LXDVirtualMachine::ipv4()
{
    if (!ip)
    {
        ip = get_ip_for(name, manager, state_url());
        if (!ip)
            return "UNKNOWN";
    }

    return ip.value().as_string();
}

std::string mp::LXDVirtualMachine::ipv6()
{
    return {};
}

void mp::LXDVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mpu::wait_until_ssh_up(this, timeout, std::bind(&LXDVirtualMachine::ensure_vm_is_running, this));
}

const QUrl mp::LXDVirtualMachine::url()
{
    return QString("%1/containers/%2").arg(base_url.toString()).arg(name);
}

const QUrl mp::LXDVirtualMachine::state_url()
{
    return url().toString() + "/state";
}

const QJsonObject mp::LXDVirtualMachine::request_state(const QString& new_state)
{
    const QJsonObject state_json{{"action", new_state}};

    return lxd_request(manager, "PUT", state_url(), state_json, 5000, false);
}

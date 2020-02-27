/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include "hyperkit_virtual_machine.h"

#include "vmprocess.h"

#include <multipass/exceptions/start_exception.h>
#include <multipass/format.h>
#include <multipass/ip_address.h>
#include <multipass/logging/log.h>
#include <multipass/optional.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <QEventLoop>
#include <QMetaObject>
#include <QString>
#include <QTimer>

#include <fstream>
#include <regex>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const mp::Path leases_path{"/var/db/dhcpd_leases"};

mp::optional<mp::IPAddress> get_ip_for(const std::string& hw_addr, std::istream& data)
{
    // bootpd leases entries consist of:
    // {
    //        name=<name>
    //        ip_address=<ipv4>
    //        hw_address=1,<mac addr>
    //        identifier=1,<mac addr>
    //        lease=<lease expiration timestamp in hex>
    // }
    const std::regex name_re{fmt::format("\\s*name={}", hw_addr)};
    const std::regex ipv4_re{"\\s*ip_address=(.+)"};
    const std::regex known_lines{"^\\s*($|\\}$|name=|hw_address=|identifier=|lease=)"};

    std::string line;
    std::smatch match;
    bool name_matched = false;
    mp::optional<mp::IPAddress> ip_address;

    while (getline(data, line))
    {
        if (line == "{")
        {
            name_matched = false;
            ip_address = mp::nullopt;
        }
        else if (regex_match(line, name_re))
            name_matched = true;
        else if (regex_match(line, match, ipv4_re))
            ip_address.emplace(match[1]);
        else if (line == "}" && name_matched && !ip_address)
            throw std::runtime_error("Failed to parse IP address out of the leases file.");
        else if (!regex_search(line, known_lines))
            mpl::log(mpl::Level::warning, "hyperkit",
                     fmt::format("Got unexpected line when parsing the leases file: {}", line));

        if (name_matched && ip_address)
            return ip_address;
    }
    return mp::nullopt;
}
} // namespace

mp::HyperkitVirtualMachine::HyperkitVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor)
    : VirtualMachine{State::off, desc.vm_name},
      monitor{&monitor},
      username{desc.ssh_username},
      desc{desc},
      update_shutdown_status{true}
{
    thread.setObjectName("HyperkitVirtualMachine thread");
}

mp::HyperkitVirtualMachine::~HyperkitVirtualMachine()
{
    update_shutdown_status = false;
    shutdown();
}

void mp::HyperkitVirtualMachine::start()
{
    if (state == State::off)
    {
        vm_process = std::make_unique<VMProcess>();

        vm_process->moveToThread(&thread);

        QObject::connect(vm_process.get(), &VMProcess::started, [=]() { on_start(); });
        QObject::connect(vm_process.get(), &VMProcess::stopped, [=](bool) { thread.quit(); });

        // cross-thread control
        QObject::connect(&thread, &QThread::started, vm_process.get(), [=]() { vm_process->start(desc); });
        QObject::connect(&thread, &QThread::finished, [=]() {
            if (update_shutdown_status)
            {
                on_shutdown();
            }
        });

        thread.start();
    }
}

void mp::HyperkitVirtualMachine::stop()
{
    if (state != State::off || state != State::stopped)
    {
        QMetaObject::invokeMethod(vm_process.get(), "stop");
        thread.wait(20000);
        update_state();
    }
}

void mp::HyperkitVirtualMachine::shutdown()
{
    stop();
}

void mp::HyperkitVirtualMachine::suspend()
{
    throw std::runtime_error("suspend is currently not supported");
}

void mp::HyperkitVirtualMachine::on_start()
{
    state = State::starting;
    update_state();
    monitor->on_resume();
}

void mp::HyperkitVirtualMachine::on_shutdown()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    if (state == State::starting)
    {
        state_wait.wait(lock, [this] { return state == State::off; });
    }
    else
    {
        state = State::off;
    }

    ip = mp::nullopt;
    update_state();
    lock.unlock();
    monitor->on_shutdown();
    vm_process.reset();
}

void mp::HyperkitVirtualMachine::ensure_vm_is_running()
{
    std::lock_guard<decltype(state_mutex)> lock{state_mutex};
    if (!thread.isRunning())
    {
        // Have to set 'off' here so there is an actual state change to compare to for
        // the cond var's predicate
        state = State::off;
        state_wait.notify_all();
        throw mp::StartException(vm_name, "Instance stopped while starting");
    }
}

mp::VirtualMachine::State mp::HyperkitVirtualMachine::current_state()
{
    return state;
}

void mp::HyperkitVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

int mp::HyperkitVirtualMachine::ssh_port()
{
    return 22;
}

std::string mp::HyperkitVirtualMachine::ssh_hostname()
{
    if (!ip)
    {
        auto action = [this] {
            ensure_vm_is_running();
            std::ifstream leases_file(leases_path.toStdString());
            auto result = get_ip_for(vm_name, leases_file);
            if (result)
            {
                ip.emplace(*result);
                return mp::utils::TimeoutAction::done;
            }
            else
            {
                return mp::utils::TimeoutAction::retry;
            }
        };
        auto on_timeout = [] { throw std::runtime_error("failed to determine IP address"); };
        mp::utils::try_action_for(on_timeout, std::chrono::minutes(2), action);
    }

    return ip->as_string();
}

std::string mp::HyperkitVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::HyperkitVirtualMachine::ipv4()
{
    if (!ip)
    {
        std::ifstream leases_file(leases_path.toStdString());
        auto result = get_ip_for(vm_name, leases_file);
        if (result)
            ip.emplace(*result);
        else
            return "UNKNOWN";
    }

    return ip->as_string();
}

std::string mp::HyperkitVirtualMachine::ipv6()
{
    return {};
}

void mp::HyperkitVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mp::utils::wait_until_ssh_up(this, timeout, std::bind(&HyperkitVirtualMachine::ensure_vm_is_running, this));
}

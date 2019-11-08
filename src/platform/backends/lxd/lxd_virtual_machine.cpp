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

#include <multipass/exceptions/start_exception.h>
#include <multipass/logging/log.h>
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

auto instance_state_for(const QString& name, const QUrl& base_url)
{
    auto manager{make_network_manager()};

    auto json_reply = lxd_request(manager.get(), "GET", QUrl(QString("%1/%2").arg(base_url.toString()).arg(name)));
    mpl::log(mpl::Level::debug, name.toStdString(), fmt::format("Got LXD container state: {} is {}", name, name));

    mpl::log(mpl::Level::error, name.toStdString(), fmt::format("Got unexpected LXD state."));
    return mp::VirtualMachine::State::unknown;
}
} // namespace

mp::LXDVirtualMachine::LXDVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor, const QUrl& base_url)
    : VirtualMachine{desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username},
      monitor{&monitor},
      base_url{base_url}
{
    try
    {
        instance_state_for(name, base_url);
    }
    catch(const LXDNotFoundException& e)
    {
        // Start instance here.
    }

    throw std::runtime_error("New machine Unimplemented");
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
    auto present_state = instance_state_for(name, base_url);

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
    auto present_state = instance_state_for(name, base_url);

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
    return "127.0.0.1";
}

std::string mp::LXDVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::LXDVirtualMachine::ipv4()
{
    return "127.0.0.1";
}

std::string mp::LXDVirtualMachine::ipv6()
{
    return {};
}

void mp::LXDVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mpu::wait_until_ssh_up(this, timeout, std::bind(&LXDVirtualMachine::ensure_vm_is_running, this));
}

/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include "hyperv_virtual_machine.h"
#include "powershell.h"

#include <multipass/exceptions/start_exception.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <shared/shared_backend_utils.h>

#include <fmt/format.h>

#include <winsock2.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const QString default_switch_guid{"C08CB7B8-9B3C-408E-8E30-5E16A3AEB444"};
const QString snapshot_name{"suspend"};

mp::optional<mp::IPAddress> remote_ip(const std::string& host, int port) // clang-format off
try // clang-format on
{
    mp::SSHSession session{host, port};

    sockaddr_in addr{};
    int size = sizeof(addr);
    auto socket = ssh_get_fd(session);
    const auto failed = getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &size);

    if (failed)
        return mp::nullopt;

    return mp::optional<mp::IPAddress>{ntohl(addr.sin_addr.s_addr)};
}
catch (...)
{
    return mp::nullopt;
}

auto instance_state_for(mp::PowerShell* power_shell, const QString& name)
{
    QString state;

    if (power_shell->run({"Get-VM", "-Name", name, "|", "Select-Object", "-ExpandProperty", "State"}, state))
    {
        if (state == "Running")
        {
            return mp::VirtualMachine::State::running;
        }
        else if (state == "Starting")
        {
            return mp::VirtualMachine::State::starting;
        }
        else if (state == "Saved")
        {
            return mp::VirtualMachine::State::suspended;
        }
        else if (state == "Off")
        {
            return mp::VirtualMachine::State::stopped;
        }
    }

    return mp::VirtualMachine::State::unknown;
}
} // namespace

mp::HyperVVirtualMachine::HyperVVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor)
    : VirtualMachine{desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username},
      power_shell{std::make_unique<PowerShell>(vm_name)},
      monitor{&monitor}
{
    if (!power_shell->run({"Get-VM", "-Name", name}))
    {
        auto mem_size = QString::number(desc.mem_size.in_bytes()); /* format documented in `Help(New-VM)`, under
        `-MemoryStartupBytes` option; */

        power_shell->run({QString("$switch = Get-VMSwitch -Id %1").arg(default_switch_guid)});

        power_shell->run({"New-VM", "-Name", name, "-Generation", "1", "-VHDPath", '"' + desc.image.image_path + '"', "-BootDevice",
                          "VHD", "-SwitchName", "$switch.Name", "-MemoryStartupBytes", mem_size});
        power_shell->run({"Set-VMProcessor", "-VMName", name, "-Count", QString::number(desc.num_cores)});
        power_shell->run({"Add-VMDvdDrive", "-VMName", name, "-Path", '"' + desc.cloud_init_iso + '"'});

        state = State::off;
    }
    else
    {
        state = instance_state_for(power_shell.get(), name);
    }
}

mp::HyperVVirtualMachine::~HyperVVirtualMachine()
{
    update_suspend_status = false;

    if (current_state() == State::running)
        suspend();
}

void mp::HyperVVirtualMachine::start()
{
    auto present_state = current_state();

    if (present_state == State::running)
        return;

    state = State::starting;
    update_state();

    power_shell->run({"Start-VM", "-Name", name});
}

void mp::HyperVVirtualMachine::stop()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    auto present_state = current_state();

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        power_shell->run({"Stop-VM", "-Name", name});
        state = State::stopped;
        ip = mp::nullopt;
    }
    else if (present_state == State::starting)
    {
        power_shell->run({"Stop-VM", "-Name", name, "-TurnOff"});
        state = State::off;
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
        ip = mp::nullopt;
    }
    else if (present_state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring shutdown issued while suspended"));
    }

    update_state();
    lock.unlock();
}

void mp::HyperVVirtualMachine::shutdown()
{
    stop();
}

void mp::HyperVVirtualMachine::suspend()
{
    auto present_state = instance_state_for(power_shell.get(), name);

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        power_shell->run({"Save-VM", "-Name", name});

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

mp::VirtualMachine::State mp::HyperVVirtualMachine::current_state()
{
    auto present_state = instance_state_for(power_shell.get(), name);

    if ((state == State::delayed_shutdown && present_state == State::running) || state == State::starting)
        return state;

    state = present_state;
    return state;
}

int mp::HyperVVirtualMachine::ssh_port()
{
    return 22;
}

void mp::HyperVVirtualMachine::ensure_vm_is_running()
{
    auto is_vm_running = [this] { return state != State::off; };

    mp::backend::ensure_vm_is_running_for(this, is_vm_running, "Instance shutdown during start");
}

void mp::HyperVVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

std::string mp::HyperVVirtualMachine::ssh_hostname(std::chrono::milliseconds /*timeout*/)
{
    return name.toStdString() + ".mshome.net";
}

std::string mp::HyperVVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::HyperVVirtualMachine::ipv4()
{
    if (!ip)
    {
        auto result = remote_ip(VirtualMachine::ssh_hostname(), ssh_port());
        if (result)
            ip.emplace(result.value());
    }
    return ip ? ip.value().as_string() : "UNKNOWN";
}

std::string mp::HyperVVirtualMachine::ipv6()
{
    return {};
}

void mp::HyperVVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mp::utils::wait_until_ssh_up(this, timeout, std::bind(&HyperVVirtualMachine::ensure_vm_is_running, this));
}

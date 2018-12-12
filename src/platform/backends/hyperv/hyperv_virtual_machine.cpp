/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

#include <winsock2.h>

namespace mp = multipass;

namespace
{
const QString default_switch_guid{"C08CB7B8-9B3C-408E-8E30-5E16A3AEB444"};

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
    }

    return mp::VirtualMachine::State::off;
}
} // namespace

mp::HyperVVirtualMachine::HyperVVirtualMachine(const VirtualMachineDescription& desc)
    : VirtualMachine{desc.key_provider, desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username},
      power_shell{std::make_unique<PowerShell>(vm_name)}
{
    if (!power_shell->run({"Get-VM", "-Name", name}))
    {
        auto mem_size = QString::fromStdString(desc.mem_size);
        if (mem_size.endsWith("K") || mem_size.endsWith("M") || mem_size.endsWith("G"))
            mem_size.append("B");

        if (!mem_size.endsWith("B"))
            mem_size.append("MB");

        power_shell->run({QString("$switch = Get-VMSwitch -Id %1").arg(default_switch_guid)});

        power_shell->run({"New-VM", "-Name", name, "-Generation", "1", "-VHDPath", desc.image.image_path, "-BootDevice",
                          "VHD", "-SwitchName", "$switch.Name", "-MemoryStartupBytes", mem_size});
        power_shell->run({"Set-VMProcessor", "-VMName", name, "-Count", QString::number(desc.num_cores)});
        power_shell->run({"Add-VMDvdDrive", "-VMName", name, "-Path", desc.cloud_init_iso});

        state = State::off;
    }
    else
    {
        state = instance_state_for(power_shell.get(), name);
    }
}

mp::HyperVVirtualMachine::~HyperVVirtualMachine()
{
    stop();
}

void mp::HyperVVirtualMachine::start()
{
    if (current_state() != State::running)
    {
        power_shell->run({"Start-VM", "-Name", name});
        state = State::running;
    }
}

void mp::HyperVVirtualMachine::stop()
{
    auto present_state = current_state();

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        power_shell->run({"Stop-VM", "-Name", name});
        state = State::stopped;
        ip = mp::nullopt;
    }
}

void mp::HyperVVirtualMachine::shutdown()
{
    stop();
}

void mp::HyperVVirtualMachine::suspend()
{
    throw std::runtime_error("suspend is currently not supported");
}

mp::VirtualMachine::State mp::HyperVVirtualMachine::current_state()
{
    auto present_state = instance_state_for(power_shell.get(), name);

    if (state == State::delayed_shutdown && present_state == State::running)
        return state;

    state = present_state;
    return state;
}

int mp::HyperVVirtualMachine::ssh_port()
{
    return 22;
}

void mp::HyperVVirtualMachine::update_state()
{
}

std::string mp::HyperVVirtualMachine::ssh_hostname()
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
        auto result = remote_ip(ssh_hostname(), ssh_port());
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
    mp::utils::wait_until_ssh_up(this, timeout);
}

void mp::HyperVVirtualMachine::wait_for_cloud_init(std::chrono::milliseconds timeout)
{
    mp::utils::wait_for_cloud_init(this, timeout);
}

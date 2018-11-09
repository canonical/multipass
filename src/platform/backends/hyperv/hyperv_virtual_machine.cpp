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

} // namespace
mp::HyperVVirtualMachine::HyperVVirtualMachine(const VirtualMachineDescription& desc)
    : VirtualMachine{State::off, desc.key_provider, desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username}
{
    if (!powershell_run({"Get-VM", "-Name", name}, vm_name))
    {
        auto mem_size = QString::fromStdString(desc.mem_size);
        if (mem_size.endsWith("K") || mem_size.endsWith("M") || mem_size.endsWith("G"))
            mem_size.append("B");

        if (!mem_size.endsWith("B"))
            mem_size.append("MB");

        powershell_run({"New-VM", "-Name", name, "-Generation", "1", "-VHDPath", desc.image.image_path, "-BootDevice",
                        "VHD", "-SwitchName", "\"Default Switch\"", "-MemoryStartupBytes", mem_size},
                       vm_name);
        powershell_run({"Add-VMDvdDrive", "-VMName", name, "-Path", desc.cloud_init_iso}, vm_name);
    }
}

mp::HyperVVirtualMachine::~HyperVVirtualMachine()
{
    stop();
}

void mp::HyperVVirtualMachine::start()
{
    powershell_run({"Start-VM", "-Name", name}, name.toStdString());
    state = State::running;
}

void mp::HyperVVirtualMachine::stop()
{
    powershell_run({"Stop-VM", "-Name", name}, name.toStdString());
    state = State::stopped;
    ip = mp::nullopt;
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

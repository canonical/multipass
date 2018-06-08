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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "hyperv_virtual_machine.h"
#include "powershell.h"

#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

namespace mp = multipass;

mp::HyperVVirtualMachine::HyperVVirtualMachine(const IPAddress& address, const VirtualMachineDescription& desc)
    : VirtualMachine{desc.key_provider, desc.vm_name},
      ip{address},
      name{QString::fromStdString(desc.vm_name)},
      state{State::off},
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
                        "VHD", "-SwitchName", "multipass", "-MemoryStartupBytes", mem_size},
                       vm_name);
        powershell_run({"Set-VMDvdDrive", "-VMName", name, "-Path", desc.cloud_init_iso}, vm_name);
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
}

void mp::HyperVVirtualMachine::shutdown()
{
    stop();
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
    return;
}

std::string mp::HyperVVirtualMachine::ssh_hostname()
{
    return ip.as_string();
}

std::string mp::HyperVVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::HyperVVirtualMachine::ipv4()
{
    return ip.as_string();
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

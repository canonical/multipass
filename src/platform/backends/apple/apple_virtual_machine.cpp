/*
 * Copyright (C) Canonical, Ltd.
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

#include <apple/apple_virtual_machine.h>

#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/top_catch_all.h>
#include <multipass/vm_status_monitor.h>

namespace multipass::apple
{
AppleVirtualMachine::AppleVirtualMachine(const VirtualMachineDescription& desc,
                                         VMStatusMonitor& monitor,
                                         const SSHKeyProvider& key_provider,
                                         const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir}, desc{desc}, monitor{&monitor}
{
}

AppleVirtualMachine::~AppleVirtualMachine()
{
}

void AppleVirtualMachine::start()
{
}

void AppleVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
}

void AppleVirtualMachine::suspend()
{
    monitor->on_suspend();
}

VirtualMachine::State AppleVirtualMachine::current_state()
{
    return VirtualMachine::State::unknown;
}

int AppleVirtualMachine::ssh_port()
{
    return 22;
}

std::string AppleVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    return "";
}

std::string AppleVirtualMachine::ssh_username()
{
    return desc.ssh_username;
}

std::optional<IPAddress> AppleVirtualMachine::management_ipv4()
{
    return std::nullopt;
}

void AppleVirtualMachine::handle_state_update()
{
}

void AppleVirtualMachine::update_cpus(int num_cores)
{
}

void AppleVirtualMachine::resize_memory(const MemorySize& new_size)
{
}

void AppleVirtualMachine::resize_disk(const MemorySize& new_size)
{
}
} // namespace multipass::apple

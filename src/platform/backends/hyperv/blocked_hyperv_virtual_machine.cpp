/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "blocked_hyperv_virtual_machine.h"

#include <multipass/constants.h>
#include <multipass/vm_status_monitor.h>

#include <fmt/format.h>

multipass::hyperv::BlockedHyperVVirtualMachine::BlockedHyperVVirtualMachine(
    const VirtualMachineDescription& description,
    VMStatusMonitor& monitor,
    const SSHKeyProvider& key_provider,
    AvailabilityZone& zone,
    const Path& instance_dir,
    std::string reason)
    : BaseVirtualMachine{State::unavailable,
                         description.vm_name,
                         description,
                         key_provider,
                         zone,
                         instance_dir},
      monitor{monitor},
      reason{std::move(reason)}
{
    handle_state_update();
}

[[noreturn]] void multipass::hyperv::BlockedHyperVVirtualMachine::throw_blocked() const
{
    throw std::runtime_error{
        fmt::format("Instance '{}' is blocked: {}", get_name(), reason)};
}

void multipass::hyperv::BlockedHyperVVirtualMachine::start()
{
    throw_blocked();
}

void multipass::hyperv::BlockedHyperVVirtualMachine::shutdown(ShutdownPolicy)
{
    throw_blocked();
}

void multipass::hyperv::BlockedHyperVVirtualMachine::suspend()
{
    throw_blocked();
}

void multipass::hyperv::BlockedHyperVVirtualMachine::set_available(bool)
{
}

multipass::VirtualMachine::State
multipass::hyperv::BlockedHyperVVirtualMachine::current_state()
{
    return state = State::unavailable;
}

int multipass::hyperv::BlockedHyperVVirtualMachine::ssh_port()
{
    return default_ssh_port;
}

std::string multipass::hyperv::BlockedHyperVVirtualMachine::ssh_hostname()
{
    throw_blocked();
}

std::string multipass::hyperv::BlockedHyperVVirtualMachine::ssh_username()
{
    return desc.ssh_username;
}

std::optional<multipass::IPAddress>
multipass::hyperv::BlockedHyperVVirtualMachine::management_ipv4()
{
    return std::nullopt;
}

void multipass::hyperv::BlockedHyperVVirtualMachine::handle_state_update()
{
    monitor.persist_state_for(get_name(), State::unavailable);
}

void multipass::hyperv::BlockedHyperVVirtualMachine::update_cpus(int)
{
    throw_blocked();
}

void multipass::hyperv::BlockedHyperVVirtualMachine::resize_memory(const MemorySize&)
{
    throw_blocked();
}

void multipass::hyperv::BlockedHyperVVirtualMachine::resize_disk_impl(const MemorySize&)
{
    throw_blocked();
}

void multipass::hyperv::BlockedHyperVVirtualMachine::add_network_interface(
    int,
    const std::string&,
    const NetworkInterface&)
{
    throw_blocked();
}

void multipass::hyperv::BlockedHyperVVirtualMachine::load_snapshots()
{
}

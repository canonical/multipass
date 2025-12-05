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

    mpl::debug(log_category, "Created handle for VM '{}'", vm_name);
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

void AppleVirtualMachine::set_state(apple::AppleVMState vm_state)
{
    mpl::debug(log_category, "set_state() -> VM `{}` VZ state `{}`", vm_name, vm_state);

    const auto prev_state = state;
    switch (vm_state)
    {
    case apple::AppleVMState::stopped:
        state = State::stopped;
        break;
    case apple::AppleVMState::running:
    case apple::AppleVMState::stopping:
        state = State::running;
        break;
    case apple::AppleVMState::paused:
        state = State::suspended;
        break;
    case apple::AppleVMState::error:
        state = State::unknown;
        break;
    case apple::AppleVMState::starting:
    case apple::AppleVMState::resuming:
    case apple::AppleVMState::restoring:
        state = State::starting;
        break;
    case apple::AppleVMState::pausing:
    case apple::AppleVMState::saving:
        state = State::suspending;
        break;
    }

    if (state == prev_state)
        return;

    mpl::info(log_category,
              "set_state() > VM {} state changed from {} to {}",
              vm_name,
              prev_state,
              state);
}
} // namespace multipass::apple

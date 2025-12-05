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

#include <applevz/applevz_virtual_machine.h>

#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/top_catch_all.h>
#include <multipass/vm_status_monitor.h>

namespace mpl = multipass::logging;

namespace
{
constexpr static auto log_category = "applevz-vm";
} // namespace

namespace multipass::applevz
{
AppleVZVirtualMachine::AppleVZVirtualMachine(const VirtualMachineDescription& desc,
                                             VMStatusMonitor& monitor,
                                             const SSHKeyProvider& key_provider,
                                             const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir}, desc{desc}, monitor{&monitor}
{
    vm_handle.reset();
    if (const auto& error = MP_APPLEVZ.create_vm(desc, vm_handle); error)
    {
        mpl::error(log_category, "Failed to create handle for VM '{}': ", vm_name, error);
    }

    mpl::debug(log_category,
               "AppleVZVirtualMachine::AppleVZVirtualMachine() -> Created handle for VM '{}'",
               vm_name);

    // Reflect compute system's state
    const auto curr_state = MP_APPLEVZ.get_state(vm_handle);
    set_state(curr_state);
    handle_state_update();
}

void AppleVZVirtualMachine::start()
{
}

void AppleVZVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
}

void AppleVZVirtualMachine::suspend()
{
    monitor->on_suspend();
}

VirtualMachine::State AppleVZVirtualMachine::current_state()
{
    return VirtualMachine::State::unknown;
}

int AppleVZVirtualMachine::ssh_port()
{
    return 22;
}

std::string AppleVZVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    return "";
}

std::string AppleVZVirtualMachine::ssh_username()
{
    return desc.ssh_username;
}

std::optional<IPAddress> AppleVZVirtualMachine::management_ipv4()
{
    return std::nullopt;
}

void AppleVZVirtualMachine::handle_state_update()
{
    monitor->persist_state_for(vm_name, state);
}

void AppleVZVirtualMachine::update_cpus(int num_cores)
{
}

void AppleVZVirtualMachine::resize_memory(const MemorySize& new_size)
{
}

void AppleVZVirtualMachine::resize_disk(const MemorySize& new_size)
{
}

void AppleVZVirtualMachine::set_state(applevz::AppleVMState vm_state)
{
    mpl::debug(log_category, "set_state() -> VM `{}` VZ state `{}`", vm_name, vm_state);

    const auto prev_state = state;
    switch (vm_state)
    {
    case applevz::AppleVMState::stopped:
        state = State::stopped;
        break;
    case applevz::AppleVMState::running:
    case applevz::AppleVMState::stopping:
        state = State::running;
        break;
    case applevz::AppleVMState::paused:
        state = State::suspended;
        break;
    case applevz::AppleVMState::error:
        state = State::unknown;
        break;
    case applevz::AppleVMState::starting:
    case applevz::AppleVMState::resuming:
    case applevz::AppleVMState::restoring:
        state = State::starting;
        break;
    case applevz::AppleVMState::pausing:
    case applevz::AppleVMState::saving:
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
} // namespace multipass::applevz

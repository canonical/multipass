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

namespace mpl = multipass::logging;

namespace
{
constexpr static auto log_category = "apple vm";
}

namespace multipass::apple
{
AppleVirtualMachine::AppleVirtualMachine(const VirtualMachineDescription& desc,
                                         VMStatusMonitor& monitor,
                                         const SSHKeyProvider& key_provider,
                                         const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir}, desc{desc}, monitor{&monitor}
{
    vm_handle.reset();
    if (const auto& error = MP_APPLE_VZ.create_vm(desc, vm_handle); error)
    {
        mpl::error(log_category, "Failed to create handle for VM '{}': ", vm_name, error);
    }

    mpl::debug(log_category,
               "AppleVirtualMachine::AppleVirtualMachine() -> Created handle for VM '{}'",
               vm_name);

    // Reflect vm's state
    const auto curr_state = MP_APPLE_VZ.get_state(vm_handle);
    set_state(curr_state);
    handle_state_update();
}

AppleVirtualMachine::~AppleVirtualMachine()
{
    mpl::debug(log_category,
               "AppleVirtualMachine::~AppleVirtualMachine() -> Destructing VM `{}`",
               vm_name);

    multipass::top_catch_all(vm_name, [this]() {
        if (state == State::running)
        {
            suspend();
        }
        else
        {
            shutdown();
        }
    });
}

void AppleVirtualMachine::start()
{
    mpl::debug(log_category, "start() -> Starting VM `{}`, current state {}", vm_name, state);

    state = State::starting;
    handle_state_update();

    CFError error;
    auto curr_state = MP_APPLE_VZ.get_state(vm_handle);

    mpl::debug(log_category, "start() -> VM `{}` VM state is `{}`", vm_name, curr_state);
    if (curr_state == apple::AppleVMState::paused && MP_APPLE_VZ.can_resume(vm_handle))
    {
        mpl::debug(log_category, "start() -> VM `{}` is in paused state, resuming", vm_name);
        error = MP_APPLE_VZ.resume_vm(vm_handle);
    }
    else if (MP_APPLE_VZ.can_start(vm_handle))
    {
        mpl::debug(log_category,
                   "start() -> VM `{}` is in {} state, starting",
                   vm_name,
                   curr_state);
        error = MP_APPLE_VZ.start_vm(vm_handle);
    }
    else
    {
        mpl::warn(log_category,
                  "start() -> VM `{}` cannot be started from state `{}`",
                  vm_name,
                  curr_state);
    }

    // Reflect vm's state
    curr_state = MP_APPLE_VZ.get_state(vm_handle);
    set_state(curr_state);
    handle_state_update();

    if (error)
    {
        mpl::error(log_category, "start() -> VM '{}' failed to start: {}", vm_name, error);
        throw std::runtime_error(
            fmt::format("VM '{}' failed to start, check logs for more details", vm_name));
    }

    mpl::debug(log_category, "start() -> VM `{}` running", vm_name);
}

void AppleVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    mpl::debug(log_category,
               "shutdown() -> Shutting down VM `{}`, current state {}",
               vm_name,
               state);

    try
    {
        check_state_for_shutdown(shutdown_policy);
    }
    catch (const VMStateIdempotentException& e)
    {
        mpl::log_message(mpl::Level::info, vm_name, e.what());
        return;
    }

    CFError error;

    if (shutdown_policy == ShutdownPolicy::Poweroff)
    {
        mpl::debug(log_category, "shutdown() -> Forcing shutdown of VM `{}`", vm_name);
        error = MP_APPLE_VZ.stop_vm(vm_handle, true);
    }
    else if (MP_APPLE_VZ.can_stop(vm_handle))
    {
        mpl::debug(log_category, "shutdown() -> Requesting shutdown of VM `{}`", vm_name);
        error = MP_APPLE_VZ.stop_vm(vm_handle);
    }
    else
    {
        mpl::warn(log_category,
                  "shutdown() -> VM `{}` cannot be stopped from state `{}`",
                  vm_name,
                  state);
        return;
    }

    // Reflect vm's state
    set_state(MP_APPLE_VZ.get_state(vm_handle));

    if (error)
    {
        mpl::error(log_category, "shutdown() -> VM '{}' failed to stop: {}", vm_name, error);
        throw std::runtime_error(
            fmt::format("VM '{}' failed to stop, check logs for more details", vm_name));
    }

    // We need to wait here.
    auto on_timeout = [] { throw std::runtime_error("timed out waiting for shutdown"); };

    multipass::utils::try_action_for(on_timeout, std::chrono::seconds{180}, [this]() {
        set_state(MP_APPLE_VZ.get_state(vm_handle));
        handle_state_update();

        switch (current_state())
        {
        case VirtualMachine::State::stopped:
            drop_ssh_session();
            return multipass::utils::TimeoutAction::done;
        default:
            return multipass::utils::TimeoutAction::retry;
        }
    });
}

void AppleVirtualMachine::suspend()
{
    monitor->on_suspend();
}

VirtualMachine::State AppleVirtualMachine::current_state()
{
    return state;
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
    monitor->persist_state_for(vm_name, state);
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

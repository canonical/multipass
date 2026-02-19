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

#include <multipass/exceptions/internal_timeout_exception.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/top_catch_all.h>
#include <multipass/vm_status_monitor.h>

#include <qemu/qemu_img_utils.h>
#include <shared/macos/backend_utils.h>

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
    initialize_vm_handle();
}

AppleVZVirtualMachine::~AppleVZVirtualMachine()
{
    mpl::debug(log_category,
               "AppleVZVirtualMachine::~AppleVZVirtualMachine() -> Destructing VM `{}`",
               vm_name);

    if (vm_handle)
    {
        multipass::top_catch_all(vm_name, [this]() {
            if (state == State::running)
            {
                suspend();
            }
            else
            {
                // TODO: since suspend to disk is not implemented yet, this will drop the vm state
                shutdown();
            }
        });
    }
}

void AppleVZVirtualMachine::start()
{
    mpl::debug(log_category, "start() -> Starting VM `{}`, current state {}", vm_name, state);

    // TODO: Drop condition after suspend to disk implemented
    if (!vm_handle)
        initialize_vm_handle();

    state = State::starting;
    handle_state_update();

    CFError error;
    if (MP_APPLEVZ.can_resume(vm_handle))
    {
        mpl::debug(log_category, "start() -> resuming VM `{}`", vm_name);
        error = MP_APPLEVZ.resume_vm(vm_handle);
    }
    else if (MP_APPLEVZ.can_start(vm_handle))
    {
        mpl::debug(log_category, "start() -> starting VM `{}`", vm_name);
        error = MP_APPLEVZ.start_vm(vm_handle);
    }
    else
    {
        mpl::error(log_category,
                   "start() -> VM `{}` cannot be started. Current state `{}`",
                   vm_name,
                   current_state());
        return;
    }

    if (error)
    {
        mpl::error(log_category, "start() -> VM '{}' failed to start: {}", vm_name, error);
        throw std::runtime_error(
            fmt::format("VM '{}' failed to start, check logs for more details", vm_name));
    }

    mpl::debug(log_category, "start() -> VM `{}` running", vm_name);
}

void AppleVZVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    std::unique_lock<std::mutex> lock{state_mutex};
    if (!vm_handle)
    {
        assert(state == State::stopped);
        return;
    }

    set_state(MP_APPLEVZ.get_state(vm_handle));

    try
    {
        check_state_for_shutdown(shutdown_policy);
    }
    catch (const VMStateIdempotentException& e)
    {
        mpl::log_message(mpl::Level::info, vm_name, e.what());
        return;
    }

    mpl::debug(log_category,
               "shutdown() -> Shutting down VM `{}`, current state {}",
               vm_name,
               state);

    CFError error;
    switch (shutdown_policy)
    {
    case ShutdownPolicy::Powerdown:
        mpl::debug(log_category, "shutdown() -> Requesting shutdown of VM `{}`", vm_name);
        if (MP_APPLEVZ.can_request_stop(vm_handle))
        {
            error = MP_APPLEVZ.stop_vm(vm_handle);
        }
        else
        {
            mpl::error(log_category,
                       "shutdown() -> VM `{}` cannot be stopped from state `{}`",
                       vm_name,
                       state);
            return;
        }
        break;
    case ShutdownPolicy::Halt:
    case ShutdownPolicy::Poweroff:
        mpl::debug(log_category, "shutdown() -> Forcing shutdown of VM `{}`", vm_name);
        if (MP_APPLEVZ.can_stop(vm_handle))
        {
            if (const auto stop_error = MP_APPLEVZ.stop_vm(vm_handle, true); stop_error)
            {
                mpl::warn(log_category,
                          "shutdown() -> VM `{}` encountered an error while quitting, killing "
                          "process instead: `{}`",
                          vm_name,
                          stop_error);
            }
        }
        else
        {
            // Go nuclear and just kill the VM process
            mpl::warn(
                log_category,
                "shutdown() -> VM `{}` cannot be stopped from state `{}`, killing process instead",
                vm_name,
                state);
        }

        drop_ssh_session();
        vm_handle.reset();
        break;
    }

    if (error)
    {
        mpl::error(log_category, "shutdown() -> VM '{}' failed to stop: {}", vm_name, error);
        throw std::runtime_error(
            fmt::format("VM '{}' failed to stop, check logs for more details", vm_name));
    }

    // We need to wait here.
    auto on_timeout = [] { throw std::runtime_error("timed out waiting for shutdown"); };

    multipass::utils::try_action_for(on_timeout, std::chrono::seconds{180}, [this]() {
        switch (current_state())
        {
        case VirtualMachine::State::stopped:
        case VirtualMachine::State::off:
            drop_ssh_session();
            vm_handle.reset();
            return multipass::utils::TimeoutAction::done;
        default:
            return multipass::utils::TimeoutAction::retry;
        }
    });
}

void AppleVZVirtualMachine::suspend()
{
    if (!vm_handle)
    {
        assert(state == State::stopped);
        return;
    }

    mpl::debug(log_category, "suspend() -> Suspending VM `{}`, current state {}", vm_name, state);

    CFError error;
    if (MP_APPLEVZ.can_pause(vm_handle))
    {
        state = State::suspending;
        handle_state_update();

        error = MP_APPLEVZ.pause_vm(vm_handle);
    }
    else
    {
        mpl::warn(log_category,
                  "suspend() -> VM `{}` cannot be suspended. Current state `{}`",
                  vm_name,
                  current_state());
    }

    if (error)
    {
        mpl::warn(log_category, "suspend() -> VM '{}' failed to pause: {}", vm_name, error);
    }

    // TODO: drop vm_handle after suspend to disk implemented
    set_state(MP_APPLEVZ.get_state(vm_handle));
}

VirtualMachine::State AppleVZVirtualMachine::current_state()
{
    // Get state from AppleVZ, translate it to our state enum, and notify the monitor
    if (!vm_handle)
        return State::stopped;
    set_state(MP_APPLEVZ.get_state(vm_handle));
    return state;
}

int AppleVZVirtualMachine::ssh_port()
{
    return 22;
}

std::string AppleVZVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    fetch_ip(timeout);

    assert(management_ip && "Should have thrown otherwise");
    return management_ip->as_string();
}

std::string AppleVZVirtualMachine::ssh_username()
{
    return desc.ssh_username;
}

std::optional<IPAddress> AppleVZVirtualMachine::management_ipv4()
{
    if (!management_ip)
        management_ip = multipass::backend::get_neighbour_ip(desc.default_mac_address);

    return management_ip;
}

void AppleVZVirtualMachine::handle_state_update()
{
    monitor->persist_state_for(vm_name, state);
}

void AppleVZVirtualMachine::update_cpus(int num_cores)
{
    assert(num_cores > 0);
    desc.num_cores = num_cores;
}

void AppleVZVirtualMachine::resize_memory(const MemorySize& new_size)
{
    desc.mem_size = new_size;
}

void AppleVZVirtualMachine::resize_disk(const MemorySize& new_size)
{
    assert(new_size > desc.disk_space);

    multipass::backend::resize_instance_image(new_size, desc.image.image_path);
    desc.disk_space = new_size;
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
        // No `stopping` state in Multipass yet
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

    handle_state_update();
}

void AppleVZVirtualMachine::fetch_ip(std::chrono::milliseconds timeout)
{
    if (management_ip)
        return;

    auto action = [this] {
        detect_aborted_start();
        return ((management_ip = multipass::backend::get_neighbour_ip(desc.default_mac_address)))
                   ? multipass::utils::TimeoutAction::done
                   : multipass::utils::TimeoutAction::retry;
    };

    auto on_timeout = [this, &timeout] {
        state = State::unknown;
        throw InternalTimeoutException{"determine IP address", timeout};
    };

    multipass::utils::try_action_for(on_timeout, timeout, action);
}

void AppleVZVirtualMachine::initialize_vm_handle()
{
    // TODO: Assert after suspend to disk implemented
    // This should only be the case if we are resuming from a paused state
    if (vm_handle)
    {
        mpl::debug(log_category,
                   "initialize_vm_handle() -> VM handle for '{}' already initialized",
                   vm_name);
        return;
    }

    mpl::trace(log_category, "initialize_vm_handle() -> Creating VM handle for '{}'", vm_name);

    vm_handle.reset();
    if (const auto& error = MP_APPLEVZ.create_vm(desc, vm_handle); error)
    {
        throw std::runtime_error(
            fmt::format("Failed to create VM handle for '{}': {}", vm_name, error));
    }

    set_state(MP_APPLEVZ.get_state(vm_handle));

    mpl::trace(log_category, "initialize_vm_handle() -> Created handle for VM '{}'", vm_name);
}
} // namespace multipass::applevz

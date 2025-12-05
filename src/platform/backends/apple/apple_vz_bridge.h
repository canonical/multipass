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

#pragma once

#include <apple/cf_error.h>

#include <multipass/virtual_machine_description.h>

namespace multipass::apple
{
using VMHandle = std::shared_ptr<void>;

enum class AppleVMState
{
    stopped,
    running,
    paused,
    error,
    starting,
    pausing,
    resuming,
    stopping,
    saving,
    restoring
};

CFError init_with_configuration(const multipass::VirtualMachineDescription& desc,
                                VMHandle& out_handle);

// Starting and stopping VM
CFError start_with_completion_handler(VMHandle& vm_handle);
CFError stop_with_completion_handler(VMHandle& vm_handle);
CFError request_stop_with_error(VMHandle& vm_handle);
CFError pause_with_completion_handler(VMHandle& vm_handle);
CFError resume_with_completion_handler(VMHandle& vm_handle);

// Getting VM state
AppleVMState get_state(VMHandle& vm_handle);

// Validate the state of VM
bool can_start(VMHandle& vm_handle);
bool can_pause(VMHandle& vm_handle);
bool can_resume(VMHandle& vm_handle);
bool can_stop(VMHandle& vm_handle);
bool can_request_stop(VMHandle& vm_handle);

} // namespace multipass::apple

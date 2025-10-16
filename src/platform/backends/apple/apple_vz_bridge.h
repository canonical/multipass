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

#include <multipass/virtual_machine_description.h>

#include <CoreFoundation/CoreFoundation.h>

namespace multipass::apple
{
using VMHandle = std::shared_ptr<void>;

extern "C"
{
CFErrorRef init_with_configuration(const multipass::VirtualMachineDescription& desc,
                                   VMHandle& out_handle);

// Starting and stopping VM
CFErrorRef start_with_completion_handler(VMHandle& vm_handle);
CFErrorRef stop_with_completion_handler(VMHandle& vm_handle);
CFErrorRef request_stop_with_error(VMHandle& vm_handle);
CFErrorRef pause_with_completion_handler(VMHandle& vm_handle);
CFErrorRef resume_with_completion_handler(VMHandle& vm_handle);
}
} // namespace multipass::apple

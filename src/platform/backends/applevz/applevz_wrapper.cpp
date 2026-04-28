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

#include <applevz/applevz_bridge.h>
#include <applevz/applevz_wrapper.h>

namespace multipass::applevz
{
CFError AppleVZ::create_vm(const VirtualMachineDescription& desc, VMHandle& out_handle) const
{
    return init_with_configuration(desc, out_handle);
}

CFError AppleVZ::start_vm(const VMHandle& vm_handle) const
{
    return start_with_completion_handler(vm_handle);
}

CFError AppleVZ::stop_vm(const VMHandle& vm_handle, bool force) const
{
    CFError err;
    if (force)
        err = stop_with_completion_handler(vm_handle);
    else
        err = request_stop_with_error(vm_handle);

    return err;
}

CFError AppleVZ::pause_vm(const VMHandle& vm_handle) const
{
    return pause_with_completion_handler(vm_handle);
}

CFError AppleVZ::resume_vm(const VMHandle& vm_handle) const
{
    return resume_with_completion_handler(vm_handle);
}

AppleVMState AppleVZ::get_state(const VMHandle& vm_handle) const
{
    return multipass::applevz::get_state(vm_handle);
}

bool AppleVZ::can_start(const VMHandle& vm_handle) const
{
    return multipass::applevz::can_start(vm_handle);
}

bool AppleVZ::can_pause(const VMHandle& vm_handle) const
{
    return multipass::applevz::can_pause(vm_handle);
}

bool AppleVZ::can_resume(const VMHandle& vm_handle) const
{
    return multipass::applevz::can_resume(vm_handle);
}

bool AppleVZ::can_stop(const VMHandle& vm_handle) const
{
    return multipass::applevz::can_stop(vm_handle);
}

bool AppleVZ::can_request_stop(const VMHandle& vm_handle) const
{
    return multipass::applevz::can_request_stop(vm_handle);
}

bool AppleVZ::is_supported() const
{
    return multipass::applevz::is_supported();
}

std::vector<NetworkInterfaceInfo> AppleVZ::bridged_network_interfaces() const
{
    return multipass::applevz::bridged_network_interfaces();
}
} // namespace multipass::applevz

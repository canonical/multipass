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

#include <multipass/logging/log.h>

namespace mpl = multipass::logging;

namespace
{
constexpr static auto kLogCategory = "vz-wrapper";
} // namespace

namespace multipass::applevz
{
CFError AppleVZ::create_vm(const VirtualMachineDescription& desc, VMHandle& out_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::create_vm(...)");

    auto err = init_with_configuration(desc, out_handle);

    if (!err)
        mpl::trace(kLogCategory, "AppleVZ::create_vm(...) succeeded");

    return err;
}

CFError AppleVZ::start_vm(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::start_vm(...)");

    auto err = start_with_completion_handler(vm_handle);

    if (!err)
        mpl::trace(kLogCategory, "AppleVZ::start_vm(...) succeeded");

    return err;
}

CFError AppleVZ::stop_vm(const VMHandle& vm_handle, bool force) const
{
    mpl::trace(kLogCategory, "AppleVZ::stop_vm(...)");

    CFError err;
    if (force)
        err = stop_with_completion_handler(vm_handle);
    else
        err = request_stop_with_error(vm_handle);

    if (!err)
        mpl::trace(kLogCategory, "AppleVZ::stop_vm(...) succeeded");

    return err;
}

CFError AppleVZ::pause_vm(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::pause_vm(...)");

    auto err = pause_with_completion_handler(vm_handle);

    if (!err)
        mpl::trace(kLogCategory, "AppleVZ::pause_vm(...) succeeded");

    return err;
}

CFError AppleVZ::resume_vm(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::resume_vm(...)");

    auto err = resume_with_completion_handler(vm_handle);

    if (!err)
        mpl::trace(kLogCategory, "AppleVZ::resume_vm(...) succeeded");

    return err;
}

AppleVMState AppleVZ::get_state(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::get_state(...)");

    return multipass::applevz::get_state(vm_handle);
}

bool AppleVZ::can_start(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::can_start(...)");

    return multipass::applevz::can_start(vm_handle);
}

bool AppleVZ::can_pause(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::can_pause(...)");

    return multipass::applevz::can_pause(vm_handle);
}

bool AppleVZ::can_resume(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::can_resume(...)");

    return multipass::applevz::can_resume(vm_handle);
}

bool AppleVZ::can_stop(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::can_stop(...)");

    return multipass::applevz::can_stop(vm_handle);
}

bool AppleVZ::can_request_stop(const VMHandle& vm_handle) const
{
    mpl::trace(kLogCategory, "AppleVZ::can_request_stop(...)");

    return multipass::applevz::can_request_stop(vm_handle);
}

bool AppleVZ::is_supported() const
{
    mpl::trace(kLogCategory, "AppleVZ::is_supported(...)");

    return multipass::applevz::is_supported();
}

bool AppleVZ::macos_at_least(int major, int minor, int patch) const
{
    mpl::trace(kLogCategory, "AppleVZ::macos_at_least(...)");

    return multipass::applevz::macos_at_least(major, minor, patch);
}
} // namespace multipass::applevz

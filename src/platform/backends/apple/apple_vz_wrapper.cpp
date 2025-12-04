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

#include <apple/apple_vz_bridge.h>
#include <apple/apple_vz_wrapper.h>

#include <multipass/logging/log.h>

namespace mpl = multipass::logging;

namespace
{
constexpr static auto kLogCategory = "vz-wrapper";
} // namespace

namespace multipass::apple
{
CFError AppleVZ::create_vm(const VirtualMachineDescription& desc, VMHandle& out_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::create_vm(...)");

    auto err = init_with_configuration(desc, out_handle);

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::create_vm(...) succeeded");

    return err;
}

CFError AppleVZ::start_vm(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::start_vm(...)");

    auto err = start_with_completion_handler(vm_handle);

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::start_vm(...) succeeded");

    return err;
}

CFError AppleVZ::stop_vm(bool force, VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::start_vm(...)");

    CFError err;
    if (force)
        err = stop_with_completion_handler(vm_handle);
    else
        err = request_stop_with_error(vm_handle);

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::stop_vm(...) succeeded");

    return err;
}

CFError AppleVZ::pause_vm(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::pause_vm(...)");

    auto err = pause_with_completion_handler(vm_handle);

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::pause_vm(...) succeeded");

    return err;
}

CFError AppleVZ::resume_vm(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::resume_vm(...)");

    auto err = resume_with_completion_handler(vm_handle);

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::resume_vm(...) succeeded");

    return err;
}

bool AppleVZ::can_start(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::can_start(...)");

    return multipass::apple::can_start(vm_handle);
}

bool AppleVZ::can_pause(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::can_pause(...)");

    return multipass::apple::can_pause(vm_handle);
}

bool AppleVZ::can_resume(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::can_resume(...)");

    return multipass::apple::can_resume(vm_handle);
}

bool AppleVZ::can_stop(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::can_stop(...)");

    return multipass::apple::can_stop(vm_handle);
}

bool AppleVZ::can_request_stop(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::can_request_stop(...)");

    return multipass::apple::can_request_stop(vm_handle);
}
} // namespace multipass::apple

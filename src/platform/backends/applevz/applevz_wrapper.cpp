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
    mpl::debug(kLogCategory, "AppleVZ::create_vm(...)");

    auto err = init_with_configuration(desc, out_handle);

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::create_vm(...) succeeded");

    return CFError(err);
}

CFError AppleVZ::start_vm(VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::start_vm(...)");

    if (!can_start(vm_handle))
        mpl::debug(kLogCategory, "VM not in a state that allows starting");

    auto err = start_with_completion_handler(vm_handle);

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::start_vm(...) succeeded");

    return CFError(err);
}

CFError AppleVZ::stop_vm(bool force, VMHandle& vm_handle) const
{
    mpl::debug(kLogCategory, "AppleVZ::start_vm(...)");

    CFError err;
    if (force)
    {
        if (!can_stop(vm_handle))
            mpl::debug(kLogCategory, "VM not in a state that allows stopping");

        err = CFError(stop_with_completion_handler(vm_handle));
    }
    else
    {
        if (!can_request_stop(vm_handle))
            mpl::debug(kLogCategory, "VM not in a state that allows stopping");

        err = CFError(request_stop_with_error(vm_handle));
    }

    if (!err)
        mpl::debug(kLogCategory, "AppleVZ::stop_vm(...) succeeded");

    return err;
}
} // namespace multipass::applevz

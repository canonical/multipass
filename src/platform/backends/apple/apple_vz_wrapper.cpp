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

    return CFError(err);
}
} // namespace multipass::apple

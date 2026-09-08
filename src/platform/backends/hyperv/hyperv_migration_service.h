/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <multipass/network_interface.h>
#include <multipass/network_interface_info.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace multipass::hyperv
{
/**
 * Resolve each source extra interface - which references a legacy Hyper-V vSwitch by id -
 * onto the single physical adapter that switch bridges, using the backend's reported
 * @p available_networks. Order and MAC address are preserved. Throws
 * @ref InstanceMigrationError if any interface's switch is unknown, or does not bridge
 * exactly one physical adapter (i.e. it is unmappable and the instance must fail rather
 * than silently lose or remap a NIC).
 */
[[nodiscard]] std::vector<NetworkInterface> translate_extra_interfaces(
    const std::vector<NetworkInterface>& source_interfaces,
    const std::vector<NetworkInterfaceInfo>& available_networks);

/**
 * Thrown for a recoverable per-instance failure. Processing
 * continues with later instances, but the final batch status becomes nonzero.
 */
class InstanceMigrationError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/**
 * Thrown when the target store is left in a state that makes further commits unsafe. The
 * batch aborts immediately; earlier committed targets are retained.
 */
class MigrationAbortError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

} // namespace multipass::hyperv

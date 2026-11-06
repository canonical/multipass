/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include <ws2tcpip.h>

#include <iphlpapi.h>

#include <optional>
#include <string>

namespace multipass
{
// Only entries on `interface_luid` are considered: the same MAC can have permanent entries on
// other interfaces, e.g. leases offered by DHCP servers of other networks on the same vSwitch.
[[nodiscard]] std::optional<std::string> permanent_ipv4_neighbor(const std::string& mac_address,
                                                                 const NET_LUID& interface_luid);
[[nodiscard]] bool remove_permanent_ipv4_neighbors(const std::string& mac_address,
                                                   const NET_LUID& interface_luid);
} // namespace multipass

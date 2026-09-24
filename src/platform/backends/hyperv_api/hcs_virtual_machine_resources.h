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

#include <optional>
#include <string>

namespace multipass::hyperv
{
[[nodiscard]] std::string endpoint_guid_for_mac(std::string mac_address);
[[nodiscard]] bool release_hcs_resources(const std::string& name);

// Permanent IPv4 neighbor entries for `mac_address`, considering only the host vNIC of the given
// HCN network. Fail (nullopt/false) if that host vNIC can't be resolved.
[[nodiscard]] std::optional<std::string> management_ipv4_neighbor(const std::string& network_guid,
                                                                  const std::string& mac_address);
[[nodiscard]] bool remove_management_ipv4_neighbors(const std::string& network_guid,
                                                    const std::string& mac_address);
} // namespace multipass::hyperv

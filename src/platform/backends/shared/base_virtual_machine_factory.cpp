/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include "base_virtual_machine_factory.h"

#include <unordered_map>

namespace mp = multipass;

std::unordered_map<mp::NetworkInterface, mp::NetworkInterfaceMatch>
mp::BaseVirtualMachineFactory::match_network_interfaces(const std::vector<NetworkInterface>& interfaces) const
{
    std::unordered_map<mp::NetworkInterface, mp::NetworkInterfaceMatch> iface_matches;

    for (const auto& iface : interfaces)
    {
        iface_matches.emplace(std::make_pair(
            iface, mp::NetworkInterfaceMatch{mp::NetworkInterfaceMatch::Type::MAC_ADDRESS, iface.mac_address}));
    }

    return iface_matches;
}

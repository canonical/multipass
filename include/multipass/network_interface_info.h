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

#ifndef MULTIPASS_NETWORK_INTERFACE_INFO_H
#define MULTIPASS_NETWORK_INTERFACE_INFO_H

#include <multipass/ip_address.h>

#include <algorithm>
#include <string>
#include <vector>

namespace multipass
{
struct NetworkInterfaceInfo
{
    bool has_link(const std::string& net) const
    {
        auto end = links.cend();
        return std::find(links.cbegin(), end, net) != end;
    }

    std::string id;
    std::string type;
    std::string description;
    std::vector<std::string> links = {}; // default initializer allows aggregate init of the other 3
    bool needs_authorization = false;    // idem
};

inline bool operator==(const NetworkInterfaceInfo& a, const NetworkInterfaceInfo& b)
{
    return std::tie(a.id, a.type, a.description, a.links, a.needs_authorization) ==
           std::tie(b.id, b.type, b.description, b.links, b.needs_authorization);
}

} // namespace multipass
#endif // MULTIPASS_NETWORK_INTERFACE_INFO_H

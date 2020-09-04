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

#ifndef MULTIPASS_NETWORK_INTERFACE_H
#define MULTIPASS_NETWORK_INTERFACE_H

#include <functional>
#include <string>
#include <unordered_map>

namespace multipass
{
struct NetworkInterface
{
    std::string id;
    std::string mac_address;
};

struct NetworkInterfaceMatch
{
    enum Type
    {
        MAC_ADDRESS,
        NAME
    };

    Type type;
    std::string value;
};
} // namespace multipass

template <>
struct std::hash<multipass::NetworkInterface>
{
    std::size_t operator()(const multipass::NetworkInterface& interface) const
    {
        return std::hash<std::string>{}(interface.id) ^ std::hash<std::string>{}(interface.mac_address);
    }
};

template <>
struct std::equal_to<multipass::NetworkInterface>
{
    bool operator()(const multipass::NetworkInterface& iface1, const multipass::NetworkInterface& iface2) const
    {
        return iface1.id == iface2.id && iface1.mac_address == iface2.mac_address;
    }
};

#endif // MULTIPASS_NETWORK_INTERFACE_H

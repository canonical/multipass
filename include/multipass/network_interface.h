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

#pragma once

#include <string>

#include <boost/json.hpp>

#include <fmt/format.h>

namespace multipass
{
namespace utils
{
// Forward-declare to avoid a circular dependency.
bool valid_mac_address(const std::string& mac);
} // namespace utils

struct NetworkInterface
{
    std::string id;
    std::string mac_address;
    bool auto_mode;

    friend inline bool operator==(const NetworkInterface& a, const NetworkInterface& b) = default;
};

inline void tag_invoke(const boost::json::value_from_tag&,
                       boost::json::value& json,
                       const NetworkInterface& iface)
{
    json = {{"id", iface.id},
            {"mac_address", iface.mac_address},
            {"auto_mode", iface.auto_mode}};
}

inline NetworkInterface tag_invoke(const boost::json::value_to_tag<NetworkInterface>&,
                                   const boost::json::value& json)
{
    auto mac_address = value_to<std::string>(json.at("mac_address"));
    if (!multipass::utils::valid_mac_address(mac_address))
        throw std::runtime_error(fmt::format("Invalid MAC address {}", mac_address));

    return {value_to<std::string>(json.at("id")),
            mac_address,
            value_to<bool>(json.at("auto_mode"))};
}

} // namespace multipass

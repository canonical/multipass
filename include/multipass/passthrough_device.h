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

namespace multipass
{

struct PassthroughDevice
{
    std::string pci_address;

    friend inline bool operator==(const PassthroughDevice& a, const PassthroughDevice& b) = default;
};

inline void tag_invoke(const boost::json::value_from_tag&,
                       boost::json::value& json,
                       const PassthroughDevice& device)
{
    json = {{"pci_address", device.pci_address}};
}

inline PassthroughDevice tag_invoke(const boost::json::value_to_tag<PassthroughDevice>&,
                                    const boost::json::value& json)
{
    return {value_to<std::string>(json.at("pci_address"))};
}

} // namespace multipass

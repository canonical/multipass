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

#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <boost/json.hpp>

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mpl = multipass::logging;

namespace multipass
{
using id_mappings = std::vector<std::pair<int, int>>;

enum class IdMappingType
{
    gid,
    uid
};

inline void tag_invoke(const boost::json::value_from_tag&,
                       boost::json::value& json,
                       const id_mappings::value_type& mapping,
                       IdMappingType type)
{
    if (type == IdMappingType::gid)
        json = {{"host_gid", mapping.first}, {"instance_gid", mapping.second}};
    else
        json = {{"host_uid", mapping.first}, {"instance_uid", mapping.second}};
}

inline id_mappings::value_type tag_invoke(const boost::json::value_to_tag<id_mappings::value_type>&,
                                          const boost::json::value& json,
                                          IdMappingType type)
{
    if (type == IdMappingType::gid)
        return {value_to<int>(json.at("host_gid")), value_to<int>(json.at("instance_gid"))};
    else
        return {value_to<int>(json.at("host_uid")), value_to<int>(json.at("instance_uid"))};
}

inline auto unique_id_mappings(id_mappings& xid_mappings)
{
    std::unordered_map<int, std::unordered_set<int>> dup_id_map;
    std::unordered_map<int, std::unordered_set<int>> dup_rev_id_map;

    for (auto it = xid_mappings.begin(); it != xid_mappings.end();)
    {
        bool duplicate = dup_id_map.find(it->first) != dup_id_map.end() ||
                         dup_rev_id_map.find(it->second) != dup_rev_id_map.end();

        dup_id_map[it->first].insert(it->second);
        dup_rev_id_map[it->second].insert(it->first);

        if (duplicate)
        {
            mpl::debug("id_mappings", "Dropping repeated mapping {}:{}", it->first, it->second);
            it = xid_mappings.erase(it);
        }
        else
        {
            ++it;
        }
    }

    auto filter_non_repeating = [](auto& map) {
        for (auto it = map.begin(); it != map.end();)
        {
            if (it->second.size() <= 1)
                it = map.erase(it);
            else
                ++it;
        }
    };

    filter_non_repeating(dup_id_map);
    filter_non_repeating(dup_rev_id_map);

    return std::make_pair(dup_id_map, dup_rev_id_map);
}
} // namespace multipass

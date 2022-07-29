/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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

#ifndef MULTIPASS_ID_MAPPINGS_H
#define MULTIPASS_ID_MAPPINGS_H

#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <set>
#include <utility>
#include <vector>

namespace mpl = multipass::logging;

namespace multipass
{
typedef typename std::vector<std::pair<int, int>> id_mappings;

inline id_mappings unique_id_mappings(const id_mappings& xid_mappings)
{
    id_mappings ret;
    std::set<std::pair<int, int>> id_set;

    for (const auto& id_map : xid_mappings)
        if (id_set.insert(id_map).second)
            ret.push_back(id_map);
        else
            mpl::log(mpl::Level::debug, "id_mappings",
                     fmt::format("Not inserting repeated map {}:{}", id_map.first, id_map.second));

    return ret;
}
} // namespace multipass

#endif // MULTIPASS_ID_MAPPINGS_H

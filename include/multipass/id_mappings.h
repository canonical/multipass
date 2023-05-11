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

#ifndef MULTIPASS_ID_MAPPINGS_H
#define MULTIPASS_ID_MAPPINGS_H

#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>
#include <vector>

namespace mpl = multipass::logging;

namespace multipass
{
using id_mappings = std::vector<std::pair<int, int>>;

inline id_mappings unique_id_mappings(const id_mappings& xid_mappings)
{
    id_mappings ret;
    std::set<std::pair<int, int>> id_set;

    auto is_mapping_repeated = [&id_set](const auto& m) {
        if (id_set.insert(m).second)
            return false;

        mpl::log(mpl::Level::debug, "id_mappings", fmt::format("Dropping repeated mapping {}:{}", m.first, m.second));

        return true;
    };

    std::remove_copy_if(xid_mappings.cbegin(), xid_mappings.cend(), std::back_insert_iterator(ret),
                        is_mapping_repeated);

    return ret;
}
} // namespace multipass

#endif // MULTIPASS_ID_MAPPINGS_H

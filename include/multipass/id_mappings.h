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

inline auto unique_id_mappings(id_mappings& xid_mappings)
{
    std::set<int> id_set;
    std::set<int> rev_id_set;

    auto is_mapping_repeated = [&id_set, &rev_id_set](const auto& m) {
        if (id_set.insert(m.first).second && rev_id_set.insert(m.second).second)
            return false;

        mpl::log(mpl::Level::debug, "id_mappings", fmt::format("Dropping repeated mapping {}:{}", m.first, m.second));
        return true;
    };

    return std::remove_if(xid_mappings.begin(), xid_mappings.end(), is_mapping_repeated);
}
} // namespace multipass

#endif // MULTIPASS_ID_MAPPINGS_H

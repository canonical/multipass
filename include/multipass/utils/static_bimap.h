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

#include <unordered_map>

namespace multipass
{

/**
 * Inefficient, static bidirectional map using two std::unordered_map's.
 */
template <typename K, typename V>
struct static_bimap
{
    static_bimap(std::initializer_list<std::pair<const K, V>> init)
        : left(init), right([&init]() {
              std::unordered_map<V, K> map;
              for (const auto& [k, v] : init)
                  map.emplace(v, k);
              return map;
          }())
    {
    }

    const std::unordered_map<K, V> left;
    const std::unordered_map<V, K> right;
};

} // namespace multipass

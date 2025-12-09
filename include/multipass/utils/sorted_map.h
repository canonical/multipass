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

#include <map>

namespace multipass
{
// Given an unsorted map (e.g. a `std::unordered_map`), return a sorted copy.
// TODO: Replace with `std::ranges:to<std::map>(x)` when we upgrade to C++23.
template <typename T>
std::map<typename T::key_type, typename T::mapped_type> sorted_map(const T& unsorted_map)
{
    return {unsorted_map.begin(), unsorted_map.end()};
}
} // namespace multipass

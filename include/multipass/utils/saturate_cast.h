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

#include <limits>
#include <type_traits>
#include <utility>

namespace multipass
{

/**
 * Converts the value from_value to a value of type To, clamping x between the minimum and maximum
 * values of type To.
 *
 * @tparam To Type to be converted
 * @tparam From From type (auto-deduced)
 *
 * TODO: Remove when C++26 arrives.
 */
template <typename To, typename From>
requires(std::is_integral<To>::value&& std::is_integral<From>::value) constexpr To
    saturate_cast(From from_value) noexcept
{
    // https://en.cppreference.com/w/cpp/numeric/saturate_cast.html
    constexpr auto min_to = std::numeric_limits<To>::min();
    constexpr auto max_to = std::numeric_limits<To>::max();

    if (std::cmp_less(from_value, min_to))
        return min_to;

    if (std::cmp_greater(from_value, max_to))
        return max_to;

    return static_cast<To>(from_value);
}
} // namespace multipass

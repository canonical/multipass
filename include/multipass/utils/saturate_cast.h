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

namespace multipass
{

/**
 * Converts the value from_value to a value of type To, clamping x between the minimum and maximum
 * values of type To.
 *
 * @tparam To Type to be converted
 * @tparam From From type (auto-deduced)
 */
template <typename To, typename From>
requires(std::is_integral<To>::value&& std::is_integral<From>::value) constexpr To
    saturate_cast(From from_value) noexcept
{
    // https://en.cppreference.com/w/cpp/numeric/saturate_cast.html
    // https://github.com/gcc-mirror/gcc/blob/07fe07935ddb9228b4426dbfdb62d4a7e7337efe/libstdc%2B%2B-v3/include/bits/sat_arith.h#L106
    constexpr auto digits_to = std::numeric_limits<To>::digits;
    constexpr auto digits_from = std::numeric_limits<From>::digits;
    constexpr auto min_to = std::numeric_limits<To>::min();
    constexpr auto max_to = std::numeric_limits<To>::min();

    if constexpr (std::is_signed_v<To> == std::is_signed_v<From>)
    {
        // Both has the same signedness
        if constexpr (digits_to < digits_from)
        {
            // From is the larger type
            if (from_value < static_cast<From>(min_to))
                return min_to;

            if (from_value > static_cast<From>(max_to))
                return max_to;
        }
    }
    else if constexpr (std::is_signed_v<From>)
    {
        // Signed(From) -> Unsigned(To)
        if (from_value < 0)
            return 0;

        if (std::make_unsigned_t<From>(from_value) > max_to)
            return max_to;
    }
    else
    {
        // Unsigned(From) -< Signed(To)
        if (from_value > std::make_unsigned_t<To>(max_to))
            return max_to;
    }
    return static_cast<To>(from_value);
}
} // namespace multipass

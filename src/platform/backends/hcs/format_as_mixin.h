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

#include <string_view>
#include <type_traits>

namespace multipass::hyperv
{
/**
 * Adds a free format_as function if the Derived is convertible to
 * std::string_view.
 *
 * @tparam Derived Derived class to extend.
 */
template <class Derived>
struct FormatAsMixin
{
    friend constexpr std::string_view format_as(const Derived& v) noexcept
    {
        static_assert(std::is_convertible_v<const Derived&, std::string_view>,
                      "Derived must be convertible to std::string_view");
        return v;
    }
};

} // namespace multipass::hyperv

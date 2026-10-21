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

#include <type_traits>

namespace multipass
{
// The type of `std::forward_like<Self>(std::declval<T>())`: lets explicit object member functions
// spell out their return type, e.g.
// `template <typename Self> auto get(this Self& self) -> forward_like_t<Self&, T>;`
// TODO: Define as `decltype(std::forward_like<Self>(std::declval<T>()))` once the Linux build uses
// libstdc++ >= 14.3. Before that, std::forward_like has a deduced return type, which Clang cannot
// use in a declaration like the one above (Ubuntu 24.04/core24 ships libstdc++ 14.2).
template <class Self, class T>
using forward_like_t = std::conditional_t<
    std::is_lvalue_reference_v<Self>,
    std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>, const T&, T&>,
    std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>, const T&&, T&&>>;
} // namespace multipass

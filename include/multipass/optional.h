/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_OPTIONAL_H
#define MULTIPASS_OPTIONAL_H

#include <optional>

namespace multipass
{
namespace alt
{
struct nullopt_t : public std::nullopt_t
{
    using std::nullopt_t::_Construct;
    using std::nullopt_t::nullopt_t;
};

inline constexpr nullopt_t nullopt{nullopt_t::_Construct::_Token};

template <typename T>
class optional : public std::optional<T>
{
public:
    constexpr optional(nullopt_t) noexcept
    {
    }
    using std::optional<T>::operator=;
    using std::optional<T>::optional;

private:
    constexpr const T* operator->() const;
    constexpr T* operator->();
    constexpr const T& operator*() const&;
    constexpr T& operator*() &;
    constexpr const T&& operator*() const&&;
    constexpr T&& operator*() &&;
};

template <typename _Tp>
constexpr optional<std::decay_t<_Tp>> make_optional(_Tp&& __t)
{
    return optional<std::decay_t<_Tp>>{std::forward<_Tp>(__t)};
}

template <typename _Tp, typename... _Args>
constexpr optional<_Tp> make_optional(_Args&&... __args)
{
    return optional<_Tp>{std::in_place, std::forward<_Args>(__args)...};
}

template <typename _Tp, typename _Up, typename... _Args>
constexpr optional<_Tp> make_optional(std::initializer_list<_Up> __il, _Args&&... __args)
{
    return optional<_Tp>{std::in_place, __il, std::forward<_Args>(__args)...};
}
} // namespace alt

using alt::make_optional;
using alt::nullopt;
using alt::optional;
} // namespace multipass

#endif // MULTIPASS_OPTIONAL_H

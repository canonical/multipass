/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_CALLABLE_TRAITS_H
#define MULTIPASS_CALLABLE_TRAITS_H

#include <tuple>
#include <type_traits>

namespace multipass
{
// For lambdas, look at it's functor operator
template <typename T>
struct callable_traits
    : public callable_traits<decltype(&std::remove_reference<T>::type::operator())>
{
};

template <typename ClassType, typename ReturnType, typename... Args>
struct callable_traits<ReturnType (ClassType::*)(Args...) const>
{
    using return_type = ReturnType;
    static constexpr std::size_t num_args = sizeof...(Args);

    template <std::size_t N>
    struct arg
    {
        static_assert(N < num_args,
                      "error: argument index is greater than number of arguments in function");
        using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
    };
};
}
#endif // MULTIPASS_CALLABLE_TRAITS_H

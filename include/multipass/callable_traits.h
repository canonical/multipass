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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_CALLABLE_TRAITS_H
#define MULTIPASS_CALLABLE_TRAITS_H

#include <tuple>

namespace multipass
{
// For lambdas, look at their function operator
// For functors, look at their function-call operator
template <typename T>
struct callable_traits : public callable_traits<decltype(&T::operator())>
{
};

// Deal with references and pointers
template <typename T>
struct callable_traits<T&> : public callable_traits<T>
{
};

template <typename T>
struct callable_traits<T*> : public callable_traits<T>
{
};

// Deal with member functions
template <typename ClassType, typename ReturnType, typename... Args>
struct callable_traits<ReturnType (ClassType::*)(Args...) const> : public callable_traits<ReturnType(Args...)>
{
};

template <typename ClassType, typename ReturnType, typename... Args>
struct callable_traits<ReturnType (ClassType::*)(Args...)> : public callable_traits<ReturnType(Args...)>
{
};

// Finally, the most basic function type, where all others will drain
template <typename ReturnType, typename... Args>
struct callable_traits<ReturnType(Args...)>
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

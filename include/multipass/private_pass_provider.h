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

namespace multipass
{
/**
 * Helper class to enable publicly visible functions to restrict calls to a type T.
 *
 * To use, inherit (publicly) using the CRTP idiom. Then require a PrivatePass in the intended
 * function and call providing T's `pass`. T can choose to forward the pass to its own friends,
 * extending callability selectively.
 *
 * @sa tests/unit/test_private_pass_provider.cpp
 */
template <typename T>
class PrivatePassProvider
{
public:
    virtual ~PrivatePassProvider() = default;

    class PrivatePass
    {
    private:
        constexpr PrivatePass() = default;
        friend class PrivatePassProvider;
    };

private:
    static constexpr PrivatePass pass{}; // token to prove friendship
    friend T;
};
} // namespace multipass

template <typename T>
constexpr multipass::PrivatePassProvider<T>::PrivatePass multipass::PrivatePassProvider<T>::pass;

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

#ifndef MULTIPASS_PRIVATE_PASS_PROVIDER_H
#define MULTIPASS_PRIVATE_PASS_PROVIDER_H

namespace multipass
{
/*
 * Helper class for classes that want to make a function publicly visible but
 * only callable privately and by friends. To use, inherit (publicly) using the
 * CRTP idiom. Then require a PrivatePass in the intended function, and provide
 * the only `pass` attribute privately to friends. Friends can then call the
 * method using the pass.
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
        friend class PrivatePassProvider<T>;
    };

private:
    static constexpr const PrivatePass pass{}; // token to prove friendship
    friend T;
};
} // namespace multipass

template <typename T>
constexpr const typename multipass::PrivatePassProvider<T>::PrivatePass multipass::PrivatePassProvider<T>::pass;

#endif // MULTIPASS_PRIVATE_PASS_PROVIDER_H

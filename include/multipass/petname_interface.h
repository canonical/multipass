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

#pragma once

#include "disabled_copy_move.h"

#include <memory>
#include <string>

namespace multipass
{

class PetnameInterface : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<PetnameInterface>;
    virtual ~PetnameInterface() = default;
    virtual std::string make_name() = 0;

protected:
    PetnameInterface() = default;
};

namespace petname
{
enum NumWords
{
    One,
    Two,
    Three,
};
template <char S>
concept IsValidSeparator = (S == '-' || S == '_');

namespace detail
{
PetnameInterface::UPtr make_petname_provider_impl(NumWords num_words, char separator);
} // namespace detail

// Templated functions to avoid exposing the PetnameProvider type in the interface header with
// the default arguments. The templates ensure the argument checks are done at compile-time.
template <NumWords NW = NumWords::Two, char S = '-'>
    requires IsValidSeparator<S>
PetnameInterface::UPtr make_petname_provider()
{
    return detail::make_petname_provider_impl(NW, S);
};

template <char S>
    requires IsValidSeparator<S>
PetnameInterface::UPtr make_petname_provider()
{
    return detail::make_petname_provider_impl(NumWords::Two, S);
}
} // namespace petname

} // namespace multipass

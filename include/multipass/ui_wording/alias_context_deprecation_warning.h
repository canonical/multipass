/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "deprecation_warning.h"

#include <fmt/format.h>
#include <string>
#include <utility>

// TODO@remove-prefer remove this file along with alias contexts
namespace multipass
{

inline constexpr auto deprecated_alias_contexts_feature = "the alias contexts feature";
inline constexpr auto alias_recreation_advice =
    "Use `multipass aliases` to inspect retained definitions and "
    "`multipass alias <instance>:<command> <new-name>` to recreate them in the default context.";

template <typename ContextName>
std::string make_inactive_alias_context_warning(ContextName&& context)
{
    return make_deprecation_warning(
        deprecated_alias_contexts_feature,
        fmt::format("Only the default context is active, so aliases in the '{}' context are "
                    "inactive. {}",
                    std::forward<ContextName>(context),
                    alias_recreation_advice));
}

template <typename AliasName>
std::string make_inactive_alias_warning(AliasName&& alias)
{
    return make_deprecation_warning(
        deprecated_alias_contexts_feature,
        fmt::format("Alias '{}' belongs to an inactive context and cannot be run. {}",
                    std::forward<AliasName>(alias),
                    alias_recreation_advice));
}

} // namespace multipass

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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#pragma once

#include <fmt/format.h>
#include <string>

namespace multipass::localization
{

template <typename DeprecatedFeature, typename AdviceMessage>
std::string make_deprecation_warning(DeprecatedFeature&& deprecated_feature, AdviceMessage&& advice)
{
    return fmt::format(
        "*** Warning: {} is deprecated and will be removed in an upcoming release.***\n\n"
        "{}\n\n",
        std::forward<DeprecatedFeature>(deprecated_feature),
        std::forward<AdviceMessage>(advice));
}

} // namespace multipass::localization

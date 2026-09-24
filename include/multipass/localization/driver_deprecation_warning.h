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

namespace multipass::localization
{

template <typename DeprecatedDriverName,
          typename RecommendedDriverName,
          typename RecommendedDriverId>
std::string make_driver_deprecation_warning(DeprecatedDriverName&& deprecated_name,
                                            RecommendedDriverName&& recommended_name,
                                            RecommendedDriverId&& recommended_id,
                                            bool migrationful)
{
    const auto deprecated_feature = fmt::format(
        "the {} driver",
        std::forward<DeprecatedDriverName>(deprecated_name));

    constexpr auto migrationless_advice =
        "We recommend switching to the new {0} driver as soon as possible (multipass set "
        "local.driver={1}). Your instances will not be destroyed but they will be unreachable from "
        "the new driver. You can switch back to the old driver for now, but you will need to "
        "manually recreate any instances you want to keep in the next release.";

    constexpr auto migrationful_advice =
        "When you are ready to have your instances migrated, please stop them (multipass stop "
        "--all) and switch to the new {0} driver (multipass set local.driver={1}).";

    const auto deprecation_advice = fmt::format(
        fmt::runtime(migrationful ? migrationful_advice : migrationless_advice),
        std::forward<RecommendedDriverName>(recommended_name),
        std::forward<RecommendedDriverId>(recommended_id));

    return make_deprecation_warning(deprecated_feature, deprecation_advice);
}

} // namespace multipass::localization

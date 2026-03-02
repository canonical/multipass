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

#include <hyperv_api/hcs/hyperv_hcs_modify_memory_settings.h>
#include <hyperv_api/hcs/hyperv_hcs_network_adapter.h>
#include <hyperv_api/hcs/hyperv_hcs_plan9_share_params.h>
#include <hyperv_api/hcs/hyperv_hcs_request_type.h>
#include <hyperv_api/hcs/hyperv_hcs_resource_path.h>

#include <fmt/xchar.h>

#include <variant>

namespace multipass::hyperv::hcs
{

/**
 * @brief HcsRequest type for HCS modifications
 */
struct HcsRequest
{
    HcsResourcePath resource_path;
    HcsRequestType request_type;
    std::variant<std::monostate,
                 HcsNetworkAdapter,
                 HcsModifyMemorySettings,
                 HcsAddPlan9ShareParameters,
                 HcsRemovePlan9ShareParameters>
        settings{std::monostate{}};
};

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for HcsRequest
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::HcsRequest, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::HcsRequest& param, FormatContext& ctx) const
        -> FormatContext::iterator;
};

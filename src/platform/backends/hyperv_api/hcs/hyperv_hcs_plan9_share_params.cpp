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
#include <hyperv_api/hcs/hyperv_hcs_plan9_share_params.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

#include <fmt/std.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcs::HcsAddPlan9ShareParameters;
using multipass::hyperv::hcs::HcsRemovePlan9ShareParameters;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsAddPlan9ShareParameters, Char>::format(const HcsAddPlan9ShareParameters& params,
                                                              FormatContext& ctx) const ->
    typename FormatContext::iterator
{
    constexpr static auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {{
            "Name": "{0}",
            "Path": "{1}",
            "Port": {2},
            "AccessName": "{3}",
            "Flags": {4}
        }}
    )json");
    return format_to(ctx.out(),
                     json_template.as<Char>(),
                     maybe_widen{params.name},
                     params.host_path,
                     params.port,
                     maybe_widen{params.access_name},
                     fmt::underlying(params.flags));
}

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsRemovePlan9ShareParameters, Char>::format(const HcsRemovePlan9ShareParameters& params,
                                                                 FormatContext& ctx) const ->
    typename FormatContext::iterator
{
    constexpr static auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {{
            "Name": "{0}",
            "AccessName": "{1}",
            "Port": {2}
        }}
    )json");

    return format_to(ctx.out(),
                     json_template.as<Char>(),
                     maybe_widen{params.name},
                     maybe_widen{params.access_name},
                     params.port);
}

template auto fmt::formatter<HcsAddPlan9ShareParameters, char>::format<fmt::format_context>(
    const HcsAddPlan9ShareParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcsAddPlan9ShareParameters, wchar_t>::format<fmt::wformat_context>(
    const HcsAddPlan9ShareParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

template auto fmt::formatter<HcsRemovePlan9ShareParameters, char>::format<fmt::format_context>(
    const HcsRemovePlan9ShareParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcsRemovePlan9ShareParameters, wchar_t>::format<fmt::wformat_context>(
    const HcsRemovePlan9ShareParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

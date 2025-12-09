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
#include <hyperv_api/hcs/hyperv_hcs_request.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

#include <fmt/std.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcs::HcsAddPlan9ShareParameters;
using multipass::hyperv::hcs::HcsModifyMemorySettings;
using multipass::hyperv::hcs::HcsNetworkAdapter;
using multipass::hyperv::hcs::HcsRemovePlan9ShareParameters;
using multipass::hyperv::hcs::HcsRequest;

template <typename Char>
struct HcsRequestSettingsFormatters
{

    template <typename T>
    static auto to_string(const T& v)
    {
        if constexpr (std::is_same_v<Char, char>)
        {
            return fmt::to_string(v);
        }
        else if constexpr (std::is_same_v<Char, wchar_t>)
        {
            return fmt::to_wstring(v);
        }
    }

    auto operator()(const std::monostate&) const
    {
        constexpr auto null_str = MULTIPASS_UNIVERSAL_LITERAL("null");
        return std::basic_string<Char>{null_str.as<Char>()};
    }

    auto operator()(const HcsNetworkAdapter& params) const
    {
        constexpr auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
            {{
                "EndpointId": "{0}",
                "MacAddress": "{1}",
                "InstanceId": "{0}"
            }}
        )json");

        return fmt::format(json_template.as<Char>(),
                           maybe_widen{params.endpoint_guid},
                           maybe_widen{params.mac_address});
    }

    auto operator()(const HcsModifyMemorySettings& params) const
    {
        return to_string(params.size_in_mb);
    }

    auto operator()(const HcsAddPlan9ShareParameters& params) const
    {
        return to_string(params);
    }

    auto operator()(const HcsRemovePlan9ShareParameters& params) const
    {
        return to_string(params);
    }
};

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsRequest, Char>::format(const HcsRequest& param, FormatContext& ctx) const
    -> FormatContext::iterator
{
    constexpr auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {{
            "ResourcePath": "{0}",
            "RequestType": "{1}",
            "Settings": {2}
        }}
    )json");

    return fmt::format_to(ctx.out(),
                          json_template.as<Char>(),
                          maybe_widen{param.resource_path},
                          maybe_widen{param.request_type},
                          std::visit(HcsRequestSettingsFormatters<Char>{}, param.settings));
}

template auto fmt::formatter<HcsRequest, char>::format<fmt::format_context>(
    const HcsRequest&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcsRequest, wchar_t>::format<fmt::wformat_context>(
    const HcsRequest&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

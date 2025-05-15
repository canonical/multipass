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
using multipass::hyperv::hcs::HcsModifyMemorySettings;
using multipass::hyperv::hcs::HcsNetworkAdapter;
using multipass::hyperv::hcs::HcsRequest;

template <typename Char>
struct HcsRequestSettingsFormatters
{
    auto operator()(const std::monostate&)
    {
        constexpr auto null_str = MULTIPASS_UNIVERSAL_LITERAL("null");
        return std::basic_string<Char>{null_str.as<Char>()};
    }

    auto operator()(const HcsNetworkAdapter& params)
    {
        constexpr static auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
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

    auto operator()(const HcsModifyMemorySettings& params)
    {
        constexpr static auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
            {0}
        )json");

        return fmt::format(json_template.as<Char>(), params.size_in_mb);
    }
};

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsRequest, Char>::format(const HcsRequest& param, FormatContext& ctx) const ->
    typename FormatContext::iterator
{
    constexpr static auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {{
            "ResourcePath": "{0}",
            "RequestType": "{1}",
            "Settings": {2}
        }}
    )json");

    return format_to(ctx.out(),
                     json_template.as<Char>(),
                     maybe_widen{param.resource_path},
                     maybe_widen{param.request_type},
                     std::visit(HcsRequestSettingsFormatters<Char>{}, param.settings));
}

template auto fmt::formatter<HcsRequest, char>::format<fmt::format_context>(const HcsRequest&,
                                                                            fmt::format_context&) const
    -> fmt::format_context::iterator;

template auto fmt::formatter<HcsRequest, wchar_t>::format<fmt::wformat_context>(const HcsRequest&,
                                                                                fmt::wformat_context&) const
    -> fmt::wformat_context::iterator;

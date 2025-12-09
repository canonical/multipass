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

#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcn::CreateNetworkParameters;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<CreateNetworkParameters, Char>::format(const CreateNetworkParameters& params,
                                                           FormatContext& ctx) const
    -> FormatContext::iterator
{
    constexpr static auto comma = MULTIPASS_UNIVERSAL_LITERAL(",");
    constexpr static auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
    {{
        "SchemaVersion":
        {{
            "Major": 2,
            "Minor": 2
        }},
        "Name": "{0}",
        "Type": "{1}",
        "Ipams": [
            {2}
        ],
        "Flags": {3},
        "Policies": [
            {4}
        ]
    }}
    )json");

    return fmt::format_to(ctx.out(),
                          json_template.as<Char>(),
                          maybe_widen{params.name},
                          maybe_widen{std::string{params.type}},
                          fmt::join(params.ipams, comma.as<Char>()),
                          fmt::underlying(params.flags),
                          fmt::join(params.policies, comma.as<Char>()));
}

template auto fmt::formatter<CreateNetworkParameters, char>::format<fmt::format_context>(
    const CreateNetworkParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<CreateNetworkParameters, wchar_t>::format<fmt::wformat_context>(
    const CreateNetworkParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

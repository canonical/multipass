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

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<CreateNetworkParameters, Char>::format(const CreateNetworkParameters& params,
                                                           FormatContext& ctx) const
    -> FormatContext::iterator
{
    static constexpr auto json_template = string_literal<Char>(R"json(
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

    return json_template.format_to(ctx,
                                   params.name,
                                   params.type,
                                   fmt::join(params.ipams, string_literal<Char>(",")),
                                   fmt::underlying(params.flags),
                                   fmt::join(params.policies, string_literal<Char>(",")));
}

template auto fmt::formatter<CreateNetworkParameters, char>::format<fmt::format_context>(
    const CreateNetworkParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<CreateNetworkParameters, wchar_t>::format<fmt::wformat_context>(
    const CreateNetworkParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

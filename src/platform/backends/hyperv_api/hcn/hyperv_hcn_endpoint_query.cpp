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

#include <hyperv_api/hcn/hyperv_hcn_endpoint_query.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<EndpointQuery, Char>::format(const EndpointQuery& params,
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
        "Filter": "{{\"VirtualMachine\": \"{0}\"}}"
    }}
    )json");

    auto v = json_template.format(params.vm_guid);
    std::wprintf(L"%s\n", v.c_str());

    return json_template.format_to(ctx, params.vm_guid);
}

template auto fmt::formatter<EndpointQuery, char>::format<fmt::format_context>(
    const EndpointQuery&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<EndpointQuery, wchar_t>::format<fmt::wformat_context>(
    const EndpointQuery&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

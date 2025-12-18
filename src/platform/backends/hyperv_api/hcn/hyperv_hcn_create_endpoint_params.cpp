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

#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<CreateEndpointParameters, Char>::format(const CreateEndpointParameters& params,
                                                            FormatContext& ctx) const
    -> FormatContext::iterator
{
    static constexpr auto json = string_literal<Char>(R"json(
    {{
        "SchemaVersion": {{
            "Major": 2,
            "Minor": 16
        }},
        "HostComputeNetwork": "{0}",
        "Policies": [],
        "MacAddress" : {1}
    }})json");

    return json.format_to(ctx,
                          params.network_guid,
                          std::string{params.mac_address ? params.mac_address.value() : "null"});
}

template auto fmt::formatter<CreateEndpointParameters, char>::format<fmt::format_context>(
    const CreateEndpointParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<CreateEndpointParameters, wchar_t>::format<fmt::wformat_context>(
    const CreateEndpointParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

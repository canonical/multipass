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

#include <hyperv_api/hcn/hyperv_hcn_network_info.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcnNetworkInfo, Char>::format(const HcnNetworkInfo& info, FormatContext& ctx)
    const -> FormatContext::iterator
{
    static constexpr auto json_template = string_literal<Char>(R"json(
            {{
                "Id": "{0}",
                "Name": "{1}",
                "Type": "{2}"
            }}
        )json");

    return json_template.format_to(ctx, info.guid, info.name, info.type);
}

template auto fmt::formatter<HcnNetworkInfo, char>::format<fmt::format_context>(
    const HcnNetworkInfo&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcnNetworkInfo, wchar_t>::format<fmt::wformat_context>(
    const HcnNetworkInfo&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

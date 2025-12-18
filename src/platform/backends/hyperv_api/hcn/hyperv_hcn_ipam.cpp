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

#include <hyperv_api/hcn/hyperv_hcn_ipam.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcnIpam, Char>::format(const HcnIpam& ipam, FormatContext& ctx) const
    -> FormatContext::iterator
{
    constexpr static auto subnet_template = string_literal<Char>(R"json(
        {{
            "Type": "{}",
            "Subnets": [
                {}
            ]
        }}
    )json");

    return subnet_template.format_to(ctx,
                                     ipam.type,
                                     fmt::join(ipam.subnets, string_literal<Char>(",")));
}

template auto fmt::formatter<HcnIpam, char>::format<fmt::format_context>(const HcnIpam&,
                                                                         fmt::format_context&) const
    -> fmt::format_context::iterator;

template auto fmt::formatter<HcnIpam, wchar_t>::format<fmt::wformat_context>(
    const HcnIpam&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

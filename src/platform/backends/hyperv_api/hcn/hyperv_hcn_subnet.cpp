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

#include <hyperv_api/hcn/hyperv_hcn_subnet.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcn::HcnSubnet;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcnSubnet, Char>::format(const HcnSubnet& subnet, FormatContext& ctx) const
    -> FormatContext::iterator
{
    constexpr auto subnet_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
            {{
                "Policies": [],
                "Routes" : [
                    {}
                ],
                "IpAddressPrefix" : "{}",
                "IpSubnets": null
            }}
        )json");

    constexpr auto comma = MULTIPASS_UNIVERSAL_LITERAL(",");

    return fmt::format_to(ctx.out(),
                          subnet_template.as<Char>(),
                          fmt::join(subnet.routes, comma.as<Char>()),
                          maybe_widen{subnet.ip_address_prefix});
}

template auto fmt::formatter<HcnSubnet, char>::format<fmt::format_context>(
    const HcnSubnet&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcnSubnet, wchar_t>::format<fmt::wformat_context>(
    const HcnSubnet&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

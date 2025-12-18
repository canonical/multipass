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

#include <hyperv_api/hcn/hyperv_hcn_route.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcn;
using namespace multipass::hyperv::literals;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcnRoute, Char>::format(const HcnRoute& route, FormatContext& ctx) const
    -> FormatContext::iterator
{
    static constexpr auto route_template = R"json(
        {{
            "NextHop": "{}",
            "DestinationPrefix": "{}",
            "Metric": {}
        }})json"_unv;

    return fmt::format_to(ctx.out(),
                          route_template.as<Char>(),
                          maybe_widen{route.next_hop},
                          maybe_widen(route.destination_prefix),
                          route.metric);
}

template auto fmt::formatter<HcnRoute, char>::format<fmt::format_context>(
    const HcnRoute&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcnRoute, wchar_t>::format<fmt::wformat_context>(
    const HcnRoute&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;

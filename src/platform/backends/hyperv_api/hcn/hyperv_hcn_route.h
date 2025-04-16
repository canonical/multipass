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

#ifndef MULTIPASS_HYPERV_API_HCN_ROUTE_H
#define MULTIPASS_HYPERV_API_HCN_ROUTE_H

#include <hyperv_api/hyperv_api_string_conversion.h>

#include <fmt/xchar.h>

namespace multipass::hyperv::hcn
{

struct HcnRoute
{
    /**
     * IP Address of the next hop gateway
     */
    std::string next_hop{};
    /**
     * IP Prefix in CIDR
     */
    std::string destination_prefix{};
    /**
     * Route metric
     */
    std::uint8_t metric{0};
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for HcnRoute
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HcnRoute, Char> : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::HcnRoute& route, FormatContext& ctx) const ->
        typename FormatContext::iterator;
};

#endif

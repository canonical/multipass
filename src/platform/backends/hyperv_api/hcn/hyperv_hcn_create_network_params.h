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

#ifndef MULTIPASS_HYPERV_API_HCN_CREATE_NETWORK_PARAMETERS_H
#define MULTIPASS_HYPERV_API_HCN_CREATE_NETWORK_PARAMETERS_H

#include <fmt/format.h>
#include <string>

namespace multipass::hyperv::hcn
{

/**
 * Parameters for creating a new Host Compute Network
 */
struct CreateNetworkParameters
{
    /**
     * Name for the network
     */
    std::string name{};

    /**
     * RFC4122 unique identifier for the network.
     */
    std::string guid{};

    /**
     * Subnet CIDR that defines the address space of
     * the network.
     */
    std::string subnet{};

    /**
     * The default gateway address for the network.
     */
    std::string gateway{};
};

} // namespace multipass::hyperv::hcn

/**
 *  Formatter type specialization for CreateNetworkParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::CreateNetworkParameters, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::CreateNetworkParameters& params, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "Network Name: ({}) | Network GUID: ({}) | Subnet CIDR: ({}) | Gateway Addr.: ({}) ",
                         params.name,
                         params.guid,
                         params.subnet,
                         params.gateway);
    }
};

#endif // MULTIPASS_HYPERV_API_HCN_CREATE_NETWORK_PARAMETERS_H

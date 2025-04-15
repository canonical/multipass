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

#include <hyperv_api/hcn/hyperv_hcn_ipam.h>
#include <hyperv_api/hcn/hyperv_hcn_network_flags.h>
#include <hyperv_api/hcn/hyperv_hcn_network_policy.h>
#include <hyperv_api/hcn/hyperv_hcn_network_type.h>

#include <fmt/format.h>

#include <vector>

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
     * Type of the network
     */
    HcnNetworkType type{HcnNetworkType::Ics()};

    /**
     * Flags for the network.
     */
    HcnNetworkFlags flags{HcnNetworkFlags::none};

    /**
     * RFC4122 unique identifier for the network.
     */
    std::string guid{};

    /**
     * IP Address Management
     */
    std::vector<HcnIpam> ipams{};

    /**
     * Network policies
     */
    std::vector<HcnNetworkPolicy> policies;
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
                         "Network Name: ({}) | Network Type: ({}) | Network GUID: ({}) | Flags: ({}) | IPAMs: ({}) | "
                         "Policies: ({})",
                         params.name,
                         static_cast<std::string_view>(params.type),
                         params.guid,
                         params.flags,
                         fmt::join(params.ipams, ","),
                         fmt::join(params.policies, ","));
    }
};

#endif // MULTIPASS_HYPERV_API_HCN_CREATE_NETWORK_PARAMETERS_H

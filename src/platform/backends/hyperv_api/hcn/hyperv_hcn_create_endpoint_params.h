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

#ifndef MULTIPASS_HYPERV_API_HCN_CREATE_ENDPOINT_PARAMETERS_H
#define MULTIPASS_HYPERV_API_HCN_CREATE_ENDPOINT_PARAMETERS_H

#include <fmt/format.h>
#include <string>

namespace multipass::hyperv::hcn
{

/**
 * Parameters for creating a network endpoint.
 */
struct CreateEndpointParameters
{
    /**
     * The GUID of the network that will own the endpoint.
     *
     * The network must already exist.
     */
    std::string network_guid{};

    /**
     * GUID for the new endpoint.
     *
     * Must be unique.
     */
    std::string endpoint_guid{};
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for CreateEndpointParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::CreateEndpointParameters, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::CreateEndpointParameters& params, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "Endpoint GUID: ({}) | Network GUID: ({})",
                         params.endpoint_guid,
                         params.network_guid);
    }
};

#endif // MULTIPASS_HYPERV_API_HCN_CREATE_ENDPOINT_PARAMETERS_H

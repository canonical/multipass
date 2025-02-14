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

#ifndef MULTIPASS_HYPERV_API_HCS_ADD_ENDPOINT_PARAMETERS_H
#define MULTIPASS_HYPERV_API_HCS_ADD_ENDPOINT_PARAMETERS_H

#include <fmt/format.h>
#include <string>

namespace multipass::hyperv::hcs
{

/**
 * Parameters for adding a network endpoint to
 * a Host Compute System..
 */
struct AddEndpointParameters
{
    /**
     * Name of the target host compute system
     */
    std::string target_compute_system_name;

    /**
     * GUID of the endpoint to add.
     */
    std::string endpoint_guid;

    /**
     * MAC address to assign to the NIC
     */
    std::string nic_mac_address;
};

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::AddEndpointParameters, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::AddEndpointParameters& params, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "Host Compute System Name: ({}) | Endpoint GUID: ({}) | NIC MAC Address: ({})",
                         params.target_compute_system_name,
                         params.endpoint_guid,
                         params.nic_mac_address);
    }
};

#endif

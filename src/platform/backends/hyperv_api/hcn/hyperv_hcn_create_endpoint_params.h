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

#pragma once

#include <fmt/format.h>

#include <optional>
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

    /**
     * MAC address associated with the endpoint (optional).
     *
     * HCN will auto-assign a MAC address to the endpoint when
     * not specified, where applicable.
     */
    std::optional<std::string> mac_address;
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for CreateEndpointParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::CreateEndpointParameters, Char>
    : formatter<basic_string_view<Char>, Char>
{

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::CreateEndpointParameters& params,
                FormatContext& ctx) const -> typename FormatContext::iterator;
};

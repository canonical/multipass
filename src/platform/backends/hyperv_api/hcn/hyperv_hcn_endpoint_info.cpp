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

#include <hyperv_api/hcn/hyperv_hcn_endpoint_info.h>

namespace multipass::hyperv::hcn
{
HcnEndpointInfo tag_invoke(const boost::json::value_to_tag<HcnEndpointInfo>&,
                           const boost::json::value& json)
{
    const auto& endpoint = json.as_object();
    HcnEndpointInfo info;

    if (const auto* mac_address = endpoint.if_contains("MacAddress"))
        info.mac_address = boost::json::value_to<std::string>(*mac_address);

    const auto append_address = [&info](const boost::json::value& address) {
        info.ip_addresses.emplace_back(boost::json::value_to<std::string>(address));
    };

    if (const auto* configurations = endpoint.if_contains("IpConfigurations"))
    {
        for (const auto& configuration : configurations->as_array())
        {
            if (const auto* address = configuration.as_object().if_contains("IpAddress"))
                append_address(*address);
        }
    }

    // HCN schema v2 uses IpConfigurations[].IpAddress. Top-level IPAddress belongs to
    // the legacy HNS schema-v1 endpoint projection, which some Windows versions may return.
    if (const auto* address = endpoint.if_contains("IPAddress"))
        append_address(*address);

    return info;
}
} // namespace multipass::hyperv::hcn

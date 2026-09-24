/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hcs_virtual_machine_resources.h"

#include <hyperv_api/hcn/hyperv_hcn_endpoint_naming.h>
#include <hyperv_api/hcn/hyperv_hcn_network_info.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <hyperv_api/hcs/hyperv_hcs_wrapper.h>
#include <hyperv_api/hyperv_guid.h>
#include <shared/windows/net_io_api.h>
#include <shared/windows/network_utils.h>

#include <multipass/logging/log.h>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <vector>

namespace
{
constexpr auto log_category = "HyperV-Virtual-Machine-Resources";
namespace mpl = multipass::logging;
namespace mhv = multipass::hyperv;

// LUID of the host vNIC attached to the given HCN network, e.g. "vEthernet (Default Switch)".
// Resolved on every call since host vNICs are recreated along with their network.
std::optional<NET_LUID> host_interface_luid(const std::string& network_guid)
{
    mhv::hcn::HcnNetworkInfo info{};
    if (const auto result = mhv::hcn::HCN().query_network(network_guid, info); !result)
    {
        mpl::warn(log_category, "Could not query network `{}`: {}", network_guid, result);
        return std::nullopt;
    }

    if (!info.host_interface_guid)
    {
        mpl::warn(log_category, "Network `{}` has no host interface", network_guid);
        return std::nullopt;
    }

    GUID interface_guid{};
    try
    {
        interface_guid = mhv::guid_from_string(*info.host_interface_guid);
    }
    catch (const mhv::GuidParseError& e)
    {
        mpl::warn(log_category,
                  "Invalid host interface GUID `{}` for network `{}`: {}",
                  *info.host_interface_guid,
                  network_guid,
                  e.what());
        return std::nullopt;
    }

    NET_LUID luid{};
    if (const auto error = MP_NETIOAPI.ConvertInterfaceGuidToLuid(&interface_guid, &luid);
        error != NO_ERROR)
    {
        mpl::warn(log_category,
                  "Could not get LUID for host interface `{}` of network `{}`, error code {}",
                  *info.host_interface_guid,
                  network_guid,
                  error);
        return std::nullopt;
    }

    return luid;
}
} // namespace

std::optional<std::string> multipass::hyperv::management_ipv4_neighbor(
    const std::string& network_guid,
    const std::string& mac_address)
{
    const auto luid = host_interface_luid(network_guid);
    return luid ? permanent_ipv4_neighbor(mac_address, *luid) : std::nullopt;
}

bool multipass::hyperv::remove_management_ipv4_neighbors(const std::string& network_guid,
                                                         const std::string& mac_address)
{
    const auto luid = host_interface_luid(network_guid);
    return luid && remove_permanent_ipv4_neighbors(mac_address, *luid);
}

std::optional<std::string> multipass::hyperv::network_guid_for_name(const std::string& name)
{
    std::vector<std::string> network_guids;
    if (const auto result = hcn::HCN().enumerate_networks(network_guids); !result)
    {
        mpl::warn(log_category, "Could not enumerate networks: {}", result);
        return std::nullopt;
    }

    std::vector<std::string> matches;
    for (const auto& network_guid : network_guids)
    {
        hcn::HcnNetworkInfo info{};
        if (hcn::HCN().query_network(network_guid, info) && info.name == name)
            matches.push_back(network_guid);
    }

    if (matches.size() > 1)
    {
        mpl::error(log_category,
                   "Network name `{}` is ambiguous, used by: {}",
                   name,
                   fmt::join(matches, ", "));
        return std::nullopt;
    }

    return matches.empty() ? std::nullopt : std::make_optional(matches.front());
}

std::string multipass::hyperv::endpoint_guid_for_mac(std::string mac_address)
{
    std::erase(mac_address, ':');
    std::erase(mac_address, '-');
    return fmt::format("db4bdbf0-dc14-407f-9780-{}", mac_address);
}

bool multipass::hyperv::release_hcs_resources(const std::string& name)
{
    hcs::HcsSystemHandle handle{nullptr};
    if (const auto open_result = hcs::HCS().open_compute_system(name, handle); !open_result)
    {
        if (static_cast<HRESULT>(open_result.code) == HCS_E_SYSTEM_NOT_FOUND)
        {
            mpl::info(log_category, "Host compute system '{}' is already terminated", name);
        }
        else
        {
            mpl::warn(log_category,
                      "Could not open host compute system '{}': {}",
                      name,
                      open_result);
            return false;
        }
    }
    else if (const auto terminate_result = hcs::HCS().terminate_compute_system(handle);
             !terminate_result)
    {
        mpl::warn(log_category,
                  "Could not terminate host compute system '{}': {}",
                  name,
                  terminate_result);
        return false;
    }

    std::vector<std::string> endpoints;
    if (const auto enumerate_result = hcn::HCN().find_endpoints_by_name(
            hcn::endpoint_name_for(name),
            endpoints);
        !enumerate_result)
    {
        mpl::warn(log_category,
                  "Could not enumerate endpoints for '{}': {}",
                  name,
                  enumerate_result);
        return false;
    }

    auto success = true;
    for (const auto& endpoint : endpoints)
    {
        const auto result = hcn::HCN().delete_endpoint(endpoint);
        success = result && success;
        mpl::log(result ? mpl::Level::trace : mpl::Level::warning,
                 log_category,
                 "Remove named endpoint {}: {}",
                 endpoint,
                 result.code);
    }
    return success;
}

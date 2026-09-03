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

#include <hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <hyperv_api/hcs/hyperv_hcs_wrapper.h>

#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <algorithm>
#include <vector>

namespace
{
constexpr auto log_category = "HyperV-Virtual-Machine-Resources";
namespace mpl = multipass::logging;
} // namespace

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
            return true;
        }

        mpl::warn(log_category, "Could not open host compute system '{}': {}", name, open_result);
        return false;
    }

    std::string vm_guid;
    const auto guid_result = hcs::HCS().get_compute_system_guid(handle, vm_guid);
    if (!guid_result || vm_guid.empty())
    {
        mpl::warn(log_category,
                  "Could not retrieve VM guid for '{}', skipping endpoint cleanup",
                  name);
    }

    if (const auto terminate_result = hcs::HCS().terminate_compute_system(handle);
        !terminate_result)
    {
        mpl::warn(log_category,
                  "Could not terminate host compute system '{}': {}",
                  name,
                  terminate_result);
        return false;
    }

    if (vm_guid.empty())
        return false;

    std::vector<std::string> attached_endpoints;
    if (const auto enumerate_result = hcn::HCN().enumerate_attached_endpoints(vm_guid,
                                                                              attached_endpoints);
        !enumerate_result)
    {
        mpl::warn(log_category,
                  "Could not enumerate endpoints for '{}': {}",
                  name,
                  enumerate_result);
        return false;
    }

    auto success = true;
    for (const auto& endpoint : attached_endpoints)
    {
        const auto result = hcn::HCN().delete_endpoint(endpoint);
        success = result && success;
        mpl::log(result ? mpl::Level::trace : mpl::Level::warning,
                 log_category,
                 "Remove attached endpoint {}: {}",
                 endpoint,
                 result.code);
    }
    return success;
}

bool multipass::hyperv::release_hcs_resources(const std::string& name,
                                              const std::vector<std::string>& mac_addresses)
{
    if (!release_hcs_resources(name))
        return false;

    auto success = true;
    for (const auto& mac_address : mac_addresses)
    {
        const auto endpoint = endpoint_guid_for_mac(mac_address);
        const auto result = hcn::HCN().delete_endpoint(endpoint);
        const auto absent = static_cast<HRESULT>(result.code) == HCN_E_ENDPOINT_NOT_FOUND;
        success = (result || absent) && success;
        mpl::log(result || absent ? mpl::Level::trace : mpl::Level::warning,
                 log_category,
                 "Remove deterministic endpoint {}: {}",
                 endpoint,
                 result.code);
    }
    return success;
}

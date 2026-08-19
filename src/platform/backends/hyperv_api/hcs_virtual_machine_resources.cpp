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

#include <vector>

namespace
{
constexpr auto log_category = "HyperV-Virtual-Machine-Resources";
namespace mpl = multipass::logging;
}

void multipass::hyperv::remove_hcs_resources(const std::string& name)
{
    hcs::HcsSystemHandle handle{nullptr};
    if (!hcs::HCS().open_compute_system(name, handle))
    {
        mpl::info(log_category, "Host compute system '{}' is already terminated", name);
        return;
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
        return;
    }

    if (vm_guid.empty())
        return;

    std::vector<std::string> attached_endpoints;
    if (const auto enumerate_result =
            hcn::HCN().enumerate_attached_endpoints(vm_guid, attached_endpoints);
        !enumerate_result)
    {
        mpl::warn(log_category,
                  "Could not enumerate endpoints for '{}': {}",
                  name,
                  enumerate_result);
        return;
    }

    for (const auto& endpoint : attached_endpoints)
    {
        const auto result = hcn::HCN().delete_endpoint(endpoint);
        mpl::log(result ? mpl::Level::trace : mpl::Level::warning,
                 log_category,
                 "Remove attached endpoint {}: {}",
                 endpoint,
                 result.code);
    }
}

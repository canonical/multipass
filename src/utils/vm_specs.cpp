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

#include <multipass/constants.h>
#include <multipass/exceptions/ghost_instance_exception.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::VMSpecs& specs)
{
    json = {{"num_cores", specs.num_cores},
            {"mem_size", std::to_string(specs.mem_size.in_bytes())},
            {"disk_space", std::to_string(specs.disk_space.in_bytes())},
            {"ssh_username", specs.ssh_username},
            {"state", static_cast<int>(specs.state)},
            {"deleted", specs.deleted},
            {"metadata", specs.metadata},

            // Write the networking information. Write first a field "mac_addr" containing the MAC
            // address of the default network interface. Then, write all the information about the
            // rest of the interfaces.
            {"mac_addr", specs.default_mac_address},
            {"extra_interfaces", boost::json::value_from(specs.extra_interfaces)},
            {"mounts", boost::json::value_from(specs.mounts, MapAsJsonArray{"target_path"})},
            {"clone_count", specs.clone_count},
            {"zone", specs.zone}};
}

mp::VMSpecs mp::tag_invoke(const boost::json::value_to_tag<mp::VMSpecs>&,
                           const boost::json::value& json,
                           const AvailabilityZoneManager& az_manager)
{
    auto num_cores = value_to<int>(json.at("num_cores"));
    auto mem_size = value_to<std::string>(json.at("mem_size"));
    auto disk_space = value_to<std::string>(json.at("disk_space"));
    auto mac_addr = value_to<std::string>(json.at("mac_addr"));
    auto ssh_username = value_to<std::string>(json.at("ssh_username"));
    auto deleted = value_to<bool>(json.at("deleted"));
    auto metadata = json.at("metadata").as_object();

    if (!num_cores && !deleted && ssh_username.empty() && metadata.empty() &&
        !MemorySize{mem_size}.in_bytes() && !MemorySize{disk_space}.in_bytes())
        throw GhostInstanceException();

    if (!mac_addr.empty() && !mpu::valid_mac_address(mac_addr))
        throw std::runtime_error(fmt::format("Invalid MAC address {}", mac_addr));
    if (ssh_username.empty())
        ssh_username = "ubuntu";

    using mounts_t = std::unordered_map<std::string, VMMount>;
    return {num_cores,
            MemorySize{mem_size.empty() ? default_memory_size : mem_size},
            MemorySize{disk_space.empty() ? default_disk_size : disk_space},
            mac_addr,
            lookup_or<std::vector<NetworkInterface>>(json, "extra_interfaces", {}),
            ssh_username,
            static_cast<mp::VirtualMachine::State>(value_to<int>(json.at("state"))),
            value_to<mounts_t>(json.at("mounts"), MapAsJsonArray{"target_path"}),
            deleted,
            metadata,
            lookup_or<int>(json, "clone_count", 0),
            lookup_or<std::string>(json, "zone", az_manager.get_default_zone_name())
            };
}

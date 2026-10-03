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

#include <multipass/format.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

#include <stdexcept>
#include <string>

namespace mp = multipass;

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const VirtualMachineDescription& desc)
{
    json = {
        {"num_cores", desc.num_cores},
        {"mem_size", std::to_string(desc.mem_size.in_bytes())},
        {"disk_space", std::to_string(desc.disk_space.in_bytes())},
        {"ssh_username", desc.ssh_username},
        {"image", boost::json::value_from(desc.image)},
        {"cloud_init_iso", desc.cloud_init_iso.string()},
        {"zone", desc.zone},
        {"mac_addr", desc.default_mac_address},
        {"extra_interfaces", boost::json::value_from(desc.extra_interfaces)},
        {"metadata", desc.metadata},
        {"state", static_cast<int>(desc.state)},
        {"clone_count", desc.clone_count},
        {"deleted", desc.deleted},
        {"mounts", boost::json::value_from(desc.mounts, MapAsJsonArray{"target_path"})},
    };
}

mp::VirtualMachineDescription mp::tag_invoke(
    const boost::json::value_to_tag<VirtualMachineDescription>&,
    const boost::json::value& json)
{
    VirtualMachineDescription desc{};
    desc.num_cores = value_to<int>(json.at("num_cores"));
    desc.mem_size = MemorySize{value_to<std::string>(json.at("mem_size"))};
    desc.disk_space = MemorySize{value_to<std::string>(json.at("disk_space"))};
    desc.ssh_username = value_to<std::string>(json.at("ssh_username"));
    desc.image = value_to<VMImage>(json.at("image"));
    desc.cloud_init_iso = value_to<std::filesystem::path>(json.at("cloud_init_iso"));
    desc.zone = value_to<std::string>(json.at("zone"));
    desc.default_mac_address = value_to<std::string>(json.at("mac_addr"));
    desc.extra_interfaces = value_to<std::vector<NetworkInterface>>(json.at("extra_interfaces"));
    desc.metadata = json.at("metadata").as_object();
    desc.state = static_cast<VirtualMachine::State>(value_to<int>(json.at("state")));
    desc.clone_count = value_to<int>(json.at("clone_count"));
    desc.deleted = value_to<bool>(json.at("deleted"));
    desc.mounts = value_to<std::unordered_map<std::string, VMMount>>(json.at("mounts"),
                                                                     MapAsJsonArray{"target_path"});

    if (!desc.default_mac_address.empty() && !utils::valid_mac_address(desc.default_mac_address))
        throw std::runtime_error(fmt::format("Invalid MAC address {}", desc.default_mac_address));

    return desc;
}

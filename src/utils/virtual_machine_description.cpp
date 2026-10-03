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

#include <multipass/virtual_machine_description.h>

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
        {"zone", desc.zone},
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
    desc.zone = value_to<std::string>(json.at("zone"));

    return desc;
}

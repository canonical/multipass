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

#include "availability_zone_manager.h"
#include "memory_size.h"
#include "network_interface.h"
#include "virtual_machine.h"
#include "vm_mount.h"

#include <boost/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace multipass
{
struct VMSpecs
{
    int num_cores;
    MemorySize mem_size;
    MemorySize disk_space;
    std::string default_mac_address;
    std::vector<NetworkInterface> extra_interfaces; // We want interfaces to be ordered.
    std::string ssh_username;
    VirtualMachine::State state;
    std::unordered_map<std::string, VMMount> mounts;
    bool deleted;
    boost::json::object metadata;
    int clone_count =
        0; // tracks the number of cloned vm from this source vm (regardless of deletes)
    std::string zone;

    friend inline bool operator==(const VMSpecs& a, const VMSpecs& b) = default;
};

void tag_invoke(const boost::json::value_from_tag&, boost::json::value& json, const VMSpecs& mount);
VMSpecs tag_invoke(const boost::json::value_to_tag<VMSpecs>&, const boost::json::value& json, const AvailabilityZoneManager& az_manager);

} // namespace multipass

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

#include "memory_size.h"
#include "network_interface.h"
#include "virtual_machine.h"
#include "vm_mount.h"

#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <QJsonObject>

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
    QJsonObject metadata;
    int clone_count =
        0; // tracks the number of cloned vm from this source vm (regardless of deletes)
    std::string zone;
};

inline bool operator==(const VMSpecs& a, const VMSpecs& b)
{
    const auto properties_of = [](const VMSpecs& spec) {
        return std::tuple{
            spec.num_cores,
            spec.mem_size,
            spec.disk_space,
            spec.default_mac_address,
            spec.extra_interfaces,
            spec.ssh_username,
            spec.state,
            spec.mounts,
            spec.deleted,
            spec.metadata,
            spec.clone_count,
            spec.zone,
        };
    };

    return properties_of(a) == properties_of(b);
}

inline bool operator!=(const VMSpecs& a, const VMSpecs& b) // TODO drop in C++20
{
    return !(a == b);
}

} // namespace multipass

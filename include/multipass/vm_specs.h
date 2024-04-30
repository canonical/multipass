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

#ifndef MULTIPASS_VM_SPECS_H
#define MULTIPASS_VM_SPECS_H

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
    int clone_count = 0; // tracks the number of cloned vm from this source vm (regardless of deletes)
};

inline bool operator==(const VMSpecs& a, const VMSpecs& b)
{
    return std::tie(a.num_cores,
                    a.mem_size,
                    a.disk_space,
                    a.default_mac_address,
                    a.extra_interfaces,
                    a.ssh_username,
                    a.state,
                    a.mounts,
                    a.deleted,
                    a.metadata,
                    a.clone_count) == std::tie(b.num_cores,
                                               b.mem_size,
                                               b.disk_space,
                                               b.default_mac_address,
                                               b.extra_interfaces,
                                               b.ssh_username,
                                               b.state,
                                               b.mounts,
                                               b.deleted,
                                               b.metadata,
                                               a.clone_count);
}

inline bool operator!=(const VMSpecs& a, const VMSpecs& b) // TODO drop in C++20
{
    return !(a == b);
}

} // namespace multipass

#endif // MULTIPASS_VM_SPECS_H

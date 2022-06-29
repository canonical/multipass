/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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

#include <multipass/id_mappings.h>
#include <multipass/memory_size.h>
#include <multipass/network_interface.h>
#include <multipass/virtual_machine.h>

#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <QJsonObject>

namespace multipass
{
struct VMMount
{
    enum class MountType : int
    {
        SSHFS = 0,
        Performance = 1
    };

    std::string source_path;
    id_mappings gid_mappings;
    id_mappings uid_mappings;
    MountType mount_type;
};

struct VMSpecs
{
    int num_cores;
    multipass::MemorySize mem_size;
    multipass::MemorySize disk_space;
    std::string default_mac_address;
    std::vector<multipass::NetworkInterface> extra_interfaces; // We want interfaces to be ordered.
    std::string ssh_username;
    multipass::VirtualMachine::State state;
    std::unordered_map<std::string, VMMount> mounts;
    bool deleted;
    QJsonObject metadata;
};

inline bool operator==(const VMMount& a, const VMMount& b)
{
    return std::tie(a.source_path, a.gid_mappings, a.uid_mappings) ==
           std::tie(b.source_path, b.gid_mappings, b.uid_mappings);
}

inline bool operator==(const VMSpecs& a, const VMSpecs& b)
{
    return std::tie(a.num_cores, a.mem_size, a.disk_space, a.default_mac_address, a.extra_interfaces, a.ssh_username,
                    a.state, a.mounts, a.deleted, a.metadata) ==
           std::tie(b.num_cores, b.mem_size, b.disk_space, b.default_mac_address, b.extra_interfaces, b.ssh_username,
                    b.state, b.mounts, b.deleted, b.metadata);
}
} // namespace multipass

#endif // MULTIPASS_VM_SPECS_H

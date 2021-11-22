/*
 * Copyright (C) 2021 Canonical, Ltd.
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
#include <unordered_map>
#include <vector>

#include <QJsonObject>

namespace multipass
{
struct VMMount
{
    std::string source_path;
    id_mappings gid_mappings;
    id_mappings uid_mappings;
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
} // namespace multipass

#endif // MULTIPASS_VM_SPECS_H

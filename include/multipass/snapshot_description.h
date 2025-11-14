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
#include "virtual_machine.h"
#include "vm_mount.h"

#include <string>
#include <unordered_map>
#include <vector>

#include <QDateTime>
#include <QString>

namespace multipass
{
class VirtualMachineDescription;

struct snapshot_context
{
    const VirtualMachine& vm;
    const VirtualMachineDescription& vm_desc;
};

struct SnapshotDescription
{
    SnapshotDescription(std::string name,
                        std::string comment,
                        int parent_index,
                        std::string cloud_init_instance_id,
                        int index,
                        QDateTime creation_timestamp,
                        int num_cores,
                        MemorySize mem_size,
                        MemorySize disk_space,
                        std::vector<NetworkInterface> extra_interfaces,
                        VirtualMachine::State state,
                        std::unordered_map<std::string, VMMount> mounts,
                        boost::json::object metadata,
                        bool upgraded = false);

    std::string name;
    std::string comment;
    int parent_index;

    // Having these const simplifies thread safety (see `BaseSnapshot`).
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    const std::string cloud_init_instance_id;
    const int index;
    const QDateTime creation_timestamp;
    const int num_cores;
    const MemorySize mem_size;
    const MemorySize disk_space;
    const std::vector<NetworkInterface> extra_interfaces;
    const VirtualMachine::State state;
    const std::unordered_map<std::string, VMMount> mounts;
    const boost::json::object metadata;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    // True if this was deserialized from a legacy snapshot file.
    bool upgraded;
};

void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const SnapshotDescription& desc);
SnapshotDescription tag_invoke(const boost::json::value_to_tag<SnapshotDescription>&,
                               const boost::json::value& json,
                               const snapshot_context& ctx);
} // namespace multipass

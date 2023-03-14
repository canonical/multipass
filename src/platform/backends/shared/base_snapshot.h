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

#ifndef MULTIPASS_BASE_SNAPSHOT_H
#define MULTIPASS_BASE_SNAPSHOT_H

#include <multipass/snapshot.h>

#include <multipass/memory_size.h>
#include <multipass/vm_mount.h>

#include <QJsonObject>

namespace multipass
{
class VMSpecs;

class BaseSnapshot : public Snapshot
{
public:
    BaseSnapshot(std::string name, std::string comment, const Snapshot* parent, int num_cores, MemorySize mem_size,
                 MemorySize disk_space, VirtualMachine::State state, std::unordered_map<std::string, VMMount> mounts,
                 QJsonObject metadata);

    BaseSnapshot(std::string name, std::string comment, const Snapshot* parent, const VMSpecs& specs);

private:
    std::string name;
    std::string comment;
    const Snapshot* parent;
    int num_cores;
    MemorySize mem_size;
    MemorySize disk_space;
    VirtualMachine::State state;
    std::unordered_map<std::string, VMMount> mounts;
    QJsonObject metadata;
};
} // namespace multipass

#endif // MULTIPASS_BASE_SNAPSHOT_H

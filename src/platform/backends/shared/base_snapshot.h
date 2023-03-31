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
struct VMSpecs;

class BaseSnapshot : public Snapshot
{
public:
    BaseSnapshot(const std::string& name, const std::string& comment, std::shared_ptr<const Snapshot> parent,
                 int num_cores, MemorySize mem_size, MemorySize disk_space, VirtualMachine::State state,
                 std::unordered_map<std::string, VMMount> mounts, QJsonObject metadata);

    BaseSnapshot(const std::string& name, const std::string& comment, std::shared_ptr<const Snapshot> parent,
                 const VMSpecs& specs);

    std::string get_name() const noexcept override;
    std::string get_comment() const noexcept override;
    std::shared_ptr<const Snapshot> get_parent() const noexcept override;
    int get_num_cores() const noexcept override;
    MemorySize get_mem_size() const noexcept override;
    MemorySize get_disk_space() const noexcept override;
    VirtualMachine::State get_state() const noexcept override;
    const std::unordered_map<std::string, VMMount>& get_mounts() const noexcept override;
    const QJsonObject& get_metadata() const noexcept override;

private:
    std::string name;
    std::string comment;
    std::shared_ptr<const Snapshot> parent;
    int num_cores;
    MemorySize mem_size;
    MemorySize disk_space;
    VirtualMachine::State state;
    std::unordered_map<std::string, VMMount> mounts;
    QJsonObject metadata;
};
} // namespace multipass

inline std::string multipass::BaseSnapshot::get_name() const noexcept
{
    return name;
}

inline std::string multipass::BaseSnapshot::get_comment() const noexcept
{
    return comment;
}

inline auto multipass::BaseSnapshot::get_parent() const noexcept -> std::shared_ptr<const Snapshot>
{
    return parent;
}

inline int multipass::BaseSnapshot::get_num_cores() const noexcept
{
    return num_cores;
}

inline auto multipass::BaseSnapshot::get_mem_size() const noexcept -> MemorySize
{
    return mem_size;
}

inline auto multipass::BaseSnapshot::get_disk_space() const noexcept -> MemorySize
{
    return disk_space;
}

inline auto multipass::BaseSnapshot::get_state() const noexcept -> VirtualMachine::State
{
    return state;
}

inline auto multipass::BaseSnapshot::get_mounts() const noexcept -> const std::unordered_map<std::string, VMMount>&
{
    return mounts;
}

inline const QJsonObject& multipass::BaseSnapshot::get_metadata() const noexcept
{
    return metadata;
}

#endif // MULTIPASS_BASE_SNAPSHOT_H

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

#include <shared_mutex>

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

    std::string get_name() const override;
    std::string get_comment() const override;
    std::string get_parent_name() const override;
    std::shared_ptr<const Snapshot> get_parent() const override;
    int get_num_cores() const noexcept override;
    MemorySize get_mem_size() const noexcept override;
    MemorySize get_disk_space() const noexcept override;
    VirtualMachine::State get_state() const noexcept override;
    const std::unordered_map<std::string, VMMount>& get_mounts() const noexcept override;
    const QJsonObject& get_metadata() const noexcept override;

    QJsonObject serialize() const override;

    void set_name(const std::string& n) override;
    void set_comment(const std::string& c) override;
    void set_parent(std::shared_ptr<const Snapshot> p) override;

private:
    std::string name;
    std::string comment;
    std::shared_ptr<const Snapshot> parent;

    // This class is non-copyable and having these const simplifies thread safety
    const int num_cores;                                   // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const MemorySize mem_size;                             // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const MemorySize disk_space;                           // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const VirtualMachine::State state;                     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const std::unordered_map<std::string, VMMount> mounts; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const QJsonObject metadata;                            // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    mutable std::shared_mutex mutex;
};
} // namespace multipass

inline std::string multipass::BaseSnapshot::get_name() const
{
    const std::shared_lock lock{mutex};
    return name;
}

inline std::string multipass::BaseSnapshot::get_comment() const
{
    const std::shared_lock lock{mutex};
    return comment;
}

inline std::string multipass::BaseSnapshot::get_parent_name() const
{
    const std::shared_lock lock{mutex};
    return parent ? parent->get_name() : "--";
}

inline auto multipass::BaseSnapshot::get_parent() const -> std::shared_ptr<const Snapshot>
{
    const std::shared_lock lock{mutex};
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

inline void multipass::BaseSnapshot::set_name(const std::string& n)
{
    const std::unique_lock lock{mutex};
    name = n;
}

inline void multipass::BaseSnapshot::set_comment(const std::string& c)
{
    const std::unique_lock lock{mutex};
    comment = c;
}

inline void multipass::BaseSnapshot::set_parent(std::shared_ptr<const Snapshot> p)
{
    const std::unique_lock lock{mutex};
    parent = std::move(p);
}

#endif // MULTIPASS_BASE_SNAPSHOT_H

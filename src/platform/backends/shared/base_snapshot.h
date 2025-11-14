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

#include <multipass/snapshot.h>

#include <multipass/memory_size.h>
#include <multipass/snapshot_description.h>
#include <multipass/vm_mount.h>

#include <QJsonObject>
#include <QString>

#include <boost/json.hpp>

#include <mutex>

namespace multipass
{
struct VMSpecs;

class VirtualMachineDescription;

class BaseSnapshot : public Snapshot
{
public:
    BaseSnapshot(const std::string& name,
                 const std::string& comment,
                 const std::string& cloud_init_instance_id,
                 std::shared_ptr<Snapshot> parent,
                 const VMSpecs& specs,
                 VirtualMachine& vm);
    BaseSnapshot(const QString& filename,
                 VirtualMachine& vm,
                 const VirtualMachineDescription& desc);

    int get_index() const noexcept override;
    std::string get_name() const override;
    std::string get_comment() const override;
    std::string get_cloud_init_instance_id() const noexcept override;
    QDateTime get_creation_timestamp() const noexcept override;
    int get_num_cores() const noexcept override;
    MemorySize get_mem_size() const noexcept override;
    MemorySize get_disk_space() const noexcept override;
    std::vector<NetworkInterface> get_extra_interfaces() const noexcept override;
    VirtualMachine::State get_state() const noexcept override;

    // Note that these return references - careful not to delete the snapshot while they are in use
    const std::unordered_map<std::string, VMMount>& get_mounts() const noexcept override;
    const boost::json::object& get_metadata() const noexcept override;

    std::shared_ptr<const Snapshot> get_parent() const override;
    std::shared_ptr<Snapshot> get_parent() override;
    std::string get_parents_name() const override;
    int get_parents_index() const override;

    void set_name(const std::string& n) override;
    void set_comment(const std::string& c) override;
    void set_parent(std::shared_ptr<Snapshot> p) override;

    void capture() final;
    void erase() final;
    void apply() final;

protected:
    const QString& get_id() const noexcept;

    virtual void capture_impl() = 0;
    virtual void erase_impl() = 0;
    virtual void apply_impl() = 0;

private:
    BaseSnapshot(SnapshotDescription desc,
                 std::shared_ptr<Snapshot> parent,
                 VirtualMachine& vm,
                 bool captured);
    BaseSnapshot(SnapshotDescription desc, VirtualMachine& vm, bool captured);
    BaseSnapshot(const QJsonObject& json,
                 VirtualMachine& vm,
                 const VirtualMachineDescription& desc);

    auto erase_helper();
    QString derive_snapshot_filename() const;
    QJsonObject serialize() const;
    void persist() const;

private:
    SnapshotDescription desc;
    std::shared_ptr<Snapshot> parent;

    // This class is non-copyable and having these const simplifies thread safety.
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    const std::string cloud_init_instance_id;
    const QString id;
    const QDir storage_dir;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

    bool captured;
    mutable std::recursive_mutex mutex;
};
} // namespace multipass

inline std::string multipass::BaseSnapshot::get_name() const
{
    const std::unique_lock lock{mutex};
    return desc.name;
}

inline std::string multipass::BaseSnapshot::get_comment() const
{
    const std::unique_lock lock{mutex};
    return desc.comment;
}

inline std::string multipass::BaseSnapshot::get_cloud_init_instance_id() const noexcept
{
    return desc.cloud_init_instance_id;
}

inline int multipass::BaseSnapshot::get_index() const noexcept
{
    return desc.index;
}

inline QDateTime multipass::BaseSnapshot::get_creation_timestamp() const noexcept
{
    return desc.creation_timestamp;
}

inline std::string multipass::BaseSnapshot::get_parents_name() const
{
    std::unique_lock lock{mutex};
    auto par = parent;
    lock.unlock(); // avoid locking another snapshot while locked in here

    return par ? par->get_name() : "";
}

inline int multipass::BaseSnapshot::get_parents_index() const
{
    const std::unique_lock lock{mutex};
    return parent ? parent->get_index() : 0; // this doesn't lock
}

inline auto multipass::BaseSnapshot::get_parent() const -> std::shared_ptr<const Snapshot>
{
    const std::unique_lock lock{mutex};
    return parent;
}

inline auto multipass::BaseSnapshot::get_parent() -> std::shared_ptr<Snapshot>
{
    return std::const_pointer_cast<Snapshot>(std::as_const(*this).get_parent());
}

inline int multipass::BaseSnapshot::get_num_cores() const noexcept
{
    return desc.num_cores;
}

inline auto multipass::BaseSnapshot::get_mem_size() const noexcept -> MemorySize
{
    return desc.mem_size;
}

inline auto multipass::BaseSnapshot::get_disk_space() const noexcept -> MemorySize
{
    return desc.disk_space;
}

inline auto multipass::BaseSnapshot::get_extra_interfaces() const noexcept
    -> std::vector<NetworkInterface>
{
    return desc.extra_interfaces;
}

inline auto multipass::BaseSnapshot::get_state() const noexcept -> VirtualMachine::State
{
    return desc.state;
}

inline auto multipass::BaseSnapshot::get_mounts() const noexcept
    -> const std::unordered_map<std::string, VMMount>&
{
    return desc.mounts;
}

inline const boost::json::object& multipass::BaseSnapshot::get_metadata() const noexcept
{
    return desc.metadata;
}

inline void multipass::BaseSnapshot::set_name(const std::string& n)
{
    const std::unique_lock lock{mutex};
    assert(captured && "precondition: only captured snapshots can be edited");

    desc.name = n;
    persist();
}

inline void multipass::BaseSnapshot::set_comment(const std::string& c)
{
    const std::unique_lock lock{mutex};
    assert(captured && "precondition: only captured snapshots can be edited");

    desc.comment = c;
    persist();
}

inline void multipass::BaseSnapshot::set_parent(std::shared_ptr<Snapshot> p)
{
    const std::unique_lock lock{mutex};
    assert(captured && "precondition: only captured snapshots can be edited");

    parent = std::move(p);
    desc.parent_index = parent ? parent->get_index() : 0;
    persist();
}

inline void multipass::BaseSnapshot::capture()
{
    const std::unique_lock lock{mutex};
    assert(!captured && "pre-condition: capture should only be called once, and only for snapshots "
                        "that were not loaded from disk");

    if (!captured)
    {
        captured = true;
        capture_impl();
        persist();
    }
}

inline void multipass::BaseSnapshot::apply()
{
    const std::unique_lock lock{mutex};
    apply_impl();
    // no need to persist here for the time being: only private fields of the base class are
    // persisted for now, and those cannot be affected by apply_impl (except by setters, which
    // already persist)
}

inline const QString& multipass::BaseSnapshot::get_id() const noexcept
{
    return id;
}

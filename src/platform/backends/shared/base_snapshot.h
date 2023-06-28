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

#include <mutex>

namespace multipass
{
struct VMSpecs;

class BaseSnapshot : public Snapshot
{
public:
    BaseSnapshot(const std::string& name, const std::string& comment, const VMSpecs& specs,
                 std::shared_ptr<Snapshot> parent);
    BaseSnapshot(const QJsonObject& json, VirtualMachine& vm);

    std::string get_name() const override;
    std::string get_comment() const override;
    QDateTime get_creation_timestamp() const override;
    std::string get_parent_name() const override;
    std::shared_ptr<const Snapshot> get_parent() const override;
    std::shared_ptr<Snapshot> get_parent() override;

    int get_num_cores() const noexcept override;
    MemorySize get_mem_size() const noexcept override;
    MemorySize get_disk_space() const noexcept override;
    VirtualMachine::State get_state() const noexcept override;

    // Note that these return references - careful not to delete the snapshot while they are in use
    const std::unordered_map<std::string, VMMount>& get_mounts() const noexcept override;
    const QJsonObject& get_metadata() const noexcept override;

    QJsonObject serialize() const override;

    void set_name(const std::string& n) override;
    void set_comment(const std::string& c) override;
    void set_parent(std::shared_ptr<Snapshot> p) override;

    void capture() final;
    void erase() final;
    void apply() final;

protected:
    virtual void capture_impl() = 0;
    virtual void erase_impl() = 0;
    virtual void apply_impl() = 0;

private:
    struct InnerJsonTag
    {
    };
    BaseSnapshot(InnerJsonTag, const QJsonObject& json, VirtualMachine& vm);
    BaseSnapshot(const std::string& name, const std::string& get_comment, const QDateTime& creation_timestamp,
                 int num_cores, MemorySize mem_size, MemorySize disk_space, VirtualMachine::State state,
                 std::unordered_map<std::string, VMMount> mounts, QJsonObject metadata,
                 std::shared_ptr<Snapshot> parent);

private:
    std::string name;
    std::string comment;

    // This class is non-copyable and having these const simplifies thread safety
    const QDateTime creation_timestamp;                    // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const int num_cores;                                   // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const MemorySize mem_size;                             // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const MemorySize disk_space;                           // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const VirtualMachine::State state;                     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const std::unordered_map<std::string, VMMount> mounts; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    const QJsonObject metadata;                            // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

    std::shared_ptr<Snapshot> parent;

    mutable std::recursive_mutex mutex;
};
} // namespace multipass

inline std::string multipass::BaseSnapshot::get_name() const
{
    const std::unique_lock lock{mutex};
    return name;
}

inline std::string multipass::BaseSnapshot::get_comment() const
{
    const std::unique_lock lock{mutex};
    return comment;
}

inline QDateTime multipass::BaseSnapshot::get_creation_timestamp() const
{
    return creation_timestamp;
}

inline std::string multipass::BaseSnapshot::get_parent_name() const
{
    std::unique_lock lock{mutex};
    auto par = parent;
    lock.unlock(); // avoid locking another snapshot while locked in here

    return par ? par->get_name() : "";
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

inline void multipass::BaseSnapshot::set_parent(std::shared_ptr<Snapshot> p)
{
    const std::unique_lock lock{mutex};
    parent = std::move(p);
}

inline void multipass::BaseSnapshot::capture()
{
    const std::unique_lock lock{mutex};
    capture_impl();
}

inline void multipass::BaseSnapshot::erase()
{
    const std::unique_lock lock{mutex};
    erase_impl();
}

inline void multipass::BaseSnapshot::apply()
{
    const std::unique_lock lock{mutex};
    apply_impl();
}

#endif // MULTIPASS_BASE_SNAPSHOT_H

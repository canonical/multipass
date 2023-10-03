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

#include "base_snapshot.h"
#include "daemon/vm_specs.h" // TODO@snapshots move this

#include <multipass/id_mappings.h> // TODO@snapshots may be able to drop after extracting JSON utilities
#include <multipass/vm_mount.h>

#include <QJsonArray> // TODO@snapshots may be able to drop after extracting JSON utilities

#include <stdexcept>

namespace mp = multipass;

namespace
{
const auto snapshot_template = QStringLiteral("@%1"); /* avoid colliding with suspension snapshots; prefix with a char
                                                         that can't be part of the name, to avoid confusion */

std::unordered_map<std::string, mp::VMMount> load_mounts(const QJsonArray& json)
{
    std::unordered_map<std::string, mp::VMMount> mounts;
    for (const auto& entry : json)
    {
        mp::id_mappings uid_mappings;
        mp::id_mappings gid_mappings;

        auto target_path = entry.toObject()["target_path"].toString().toStdString();
        auto source_path = entry.toObject()["source_path"].toString().toStdString();

        for (QJsonValueRef uid_entry : entry.toObject()["uid_mappings"].toArray())
        {
            uid_mappings.push_back(
                {uid_entry.toObject()["host_uid"].toInt(), uid_entry.toObject()["instance_uid"].toInt()});
        }

        for (QJsonValueRef gid_entry : entry.toObject()["gid_mappings"].toArray())
        {
            gid_mappings.push_back(
                {gid_entry.toObject()["host_gid"].toInt(), gid_entry.toObject()["instance_gid"].toInt()});
        }

        uid_mappings = mp::unique_id_mappings(uid_mappings);
        gid_mappings = mp::unique_id_mappings(gid_mappings);
        auto mount_type = mp::VMMount::MountType(entry.toObject()["mount_type"].toInt());

        mp::VMMount mount{source_path, gid_mappings, uid_mappings, mount_type};
        mounts[target_path] = mount;
    }

    return mounts;
}

std::shared_ptr<mp::Snapshot> find_parent(const QJsonObject& json, mp::VirtualMachine& vm)
{
    auto parent_name = json["parent"].toString().toStdString();
    try
    {
        return parent_name.empty() ? nullptr : vm.get_snapshot(parent_name);
    }
    catch (std::out_of_range&)
    {
        throw std::runtime_error{fmt::format("Missing snapshot parent. Snapshot name: {}; parent name: {}",
                                             json["name"].toString(),
                                             parent_name)};
    }
}
} // namespace

mp::BaseSnapshot::BaseSnapshot(int index,
                               const QDir& storage_dir,
                               const std::string& name,    // NOLINT(modernize-pass-by-value)
                               const std::string& comment, // NOLINT(modernize-pass-by-value)
                               const QDateTime& creation_timestamp,
                               int num_cores,
                               MemorySize mem_size,
                               MemorySize disk_space,
                               VirtualMachine::State state,
                               std::unordered_map<std::string, VMMount> mounts,
                               QJsonObject metadata,
                               std::shared_ptr<Snapshot> parent)
    : index{index},
      storage_dir{storage_dir},
      name{name},
      comment{comment},
      creation_timestamp{creation_timestamp},
      num_cores{num_cores},
      mem_size{mem_size},
      disk_space{disk_space},
      state{state},
      mounts{std::move(mounts)},
      metadata{std::move(metadata)},
      parent{std::move(parent)}
{
    assert(index > 0 && "snapshot indices need to start at 1");

    if (name.empty())
        throw std::runtime_error{"Snapshot names cannot be empty"};
    if (num_cores < 1)
        throw std::runtime_error{fmt::format("Invalid number of cores for snapshot: {}", num_cores)};
    if (auto mem_bytes = mem_size.in_bytes(); mem_bytes < 1)
        throw std::runtime_error{fmt::format("Invalid memory size for snapshot: {}", mem_bytes)};
    if (auto disk_bytes = disk_space.in_bytes(); disk_bytes < 1)
        throw std::runtime_error{fmt::format("Invalid disk size for snapshot: {}", disk_bytes)};
}

mp::BaseSnapshot::BaseSnapshot(const std::string& name,
                               const std::string& comment,
                               const VMSpecs& specs,
                               std::shared_ptr<Snapshot> parent,
                               VirtualMachine& vm)
    : BaseSnapshot{vm.get_snapshot_count() + 1,
                   vm.instance_directory(),
                   name,
                   comment,
                   QDateTime::currentDateTimeUtc(),
                   specs.num_cores,
                   specs.mem_size,
                   specs.disk_space,
                   specs.state,
                   specs.mounts,
                   specs.metadata,
                   std::move(parent)}
{
}

mp::BaseSnapshot::BaseSnapshot(const QJsonObject& json, VirtualMachine& vm)
    : BaseSnapshot(InnerJsonTag{}, json["snapshot"].toObject(), vm)
{
}

mp::BaseSnapshot::BaseSnapshot(InnerJsonTag, const QJsonObject& json, VirtualMachine& vm)
    : BaseSnapshot{
          0, // TODO@ricab derive index
          vm.instance_directory(),
          json["name"].toString().toStdString(),                                           // name
          json["comment"].toString().toStdString(),                                        // comment
          QDateTime::fromString(json["creation_timestamp"].toString(), Qt::ISODateWithMs), // creation_timestamp
          json["num_cores"].toInt(),                                                       // num_cores
          MemorySize{json["mem_size"].toString().toStdString()},                           // mem_size
          MemorySize{json["disk_space"].toString().toStdString()},                         // disk_space
          static_cast<mp::VirtualMachine::State>(json["state"].toInt()),                   // state
          load_mounts(json["mounts"].toArray()),                                           // mounts
          json["metadata"].toObject(),                                                     // metadata
          find_parent(json, vm)}                                                           // parent
{
}

QJsonObject multipass::BaseSnapshot::serialize() const
{
    QJsonObject ret, snapshot{};
    const std::unique_lock lock{mutex};

    snapshot.insert("name", QString::fromStdString(name));
    snapshot.insert("comment", QString::fromStdString(comment));
    snapshot.insert("creation_timestamp", creation_timestamp.toString(Qt::ISODateWithMs));
    snapshot.insert("num_cores", num_cores);
    snapshot.insert("mem_size", QString::number(mem_size.in_bytes()));
    snapshot.insert("disk_space", QString::number(disk_space.in_bytes()));
    snapshot.insert("state", static_cast<int>(state));
    snapshot.insert("metadata", metadata);
    snapshot.insert("parent", QString::fromStdString(get_parents_name()));

    // Extract mount serialization
    QJsonArray json_mounts;
    for (const auto& mount : mounts)
    {
        QJsonObject entry;
        entry.insert("source_path", QString::fromStdString(mount.second.source_path));
        entry.insert("target_path", QString::fromStdString(mount.first));

        QJsonArray uid_mappings;

        for (const auto& map : mount.second.uid_mappings)
        {
            QJsonObject map_entry;
            map_entry.insert("host_uid", map.first);
            map_entry.insert("instance_uid", map.second);

            uid_mappings.append(map_entry);
        }

        entry.insert("uid_mappings", uid_mappings);

        QJsonArray gid_mappings;

        for (const auto& map : mount.second.gid_mappings)
        {
            QJsonObject map_entry;
            map_entry.insert("host_gid", map.first);
            map_entry.insert("instance_gid", map.second);

            gid_mappings.append(map_entry);
        }

        entry.insert("gid_mappings", gid_mappings);

        entry.insert("mount_type", static_cast<int>(mount.second.mount_type));
        json_mounts.append(entry);
    }

    snapshot.insert("mounts", json_mounts);
    ret.insert("snapshot", snapshot);

    return ret;
}

QString multipass::BaseSnapshot::derive_id() const
{
    return snapshot_template.arg(get_name().c_str());
}

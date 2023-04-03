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

#include <multipass/vm_mount.h>

#include <QJsonArray> // TODO@snapshots may be able to drop after extracting JSON utilities

namespace mp = multipass;

mp::BaseSnapshot::BaseSnapshot(const std::string& name, const std::string& comment, // NOLINT(modernize-pass-by-value)
                               std::shared_ptr<const Snapshot> parent, int num_cores, MemorySize mem_size,
                               MemorySize disk_space, VirtualMachine::State state,
                               std::unordered_map<std::string, VMMount> mounts, QJsonObject metadata)
    : name{name},
      comment{comment},
      parent{std::move(parent)},
      num_cores{num_cores},
      mem_size{mem_size},
      disk_space{disk_space},
      state{state},
      mounts{std::move(mounts)},
      metadata{std::move(metadata)}
{
}

mp::BaseSnapshot::BaseSnapshot(const std::string& name, const std::string& comment,
                               std::shared_ptr<const Snapshot> parent, const VMSpecs& specs)
    : BaseSnapshot{name,        comment,      std::move(parent), specs.num_cores, specs.mem_size, specs.disk_space,
                   specs.state, specs.mounts, specs.metadata}
{
}

mp::BaseSnapshot::BaseSnapshot(const QJsonObject& json)
    : BaseSnapshot{"", "", nullptr, 0, {}, {}, {}, {}, {}} // TODO@ricab implement
{
}

QJsonObject multipass::BaseSnapshot::serialize() const
{
    QJsonObject ret, snapshot{};
    const std::shared_lock lock{mutex};

    snapshot.insert("name", QString::fromStdString(name));
    snapshot.insert("comment", QString::fromStdString(comment));
    snapshot.insert("num_cores", num_cores);
    snapshot.insert("mem_size", QString::number(mem_size.in_bytes()));
    snapshot.insert("disk_space", QString::number(disk_space.in_bytes()));
    snapshot.insert("state", static_cast<int>(state));
    snapshot.insert("metadata", metadata);

    if (parent)
        snapshot.insert("parent", QString::fromStdString(get_parent_name()));

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

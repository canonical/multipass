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

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

#include <multipass/snapshot_description.h>

#include <fmt/format.h>

namespace
{
constexpr auto max_snapshots = 9999;
} // namespace

namespace mp = multipass;

mp::SnapshotDescription::SnapshotDescription(std::string name_,
                                             std::string comment_,
                                             int parent_index_,
                                             std::string cloud_init_instance_id_,
                                             int index_,
                                             QDateTime creation_timestamp_,
                                             int num_cores_,
                                             MemorySize mem_size_,
                                             MemorySize disk_space_,
                                             std::vector<NetworkInterface> extra_interfaces_,
                                             VirtualMachine::State state_,
                                             std::unordered_map<std::string, VMMount> mounts_,
                                             boost::json::object metadata_)
    : name(std::move(name_)),
      comment(std::move(comment_)),
      parent_index(parent_index_),
      cloud_init_instance_id(std::move(cloud_init_instance_id_)),
      index(index_),
      creation_timestamp(std::move(creation_timestamp_)),
      num_cores(num_cores_),
      mem_size(mem_size_),
      disk_space(disk_space_),
      extra_interfaces(std::move(extra_interfaces_)),
      state(state_),
      mounts(std::move(mounts_)),
      metadata(std::move(metadata_))
{
    using St = VirtualMachine::State;
    if (state != St::off && state != St::stopped)
        throw std::runtime_error{
            fmt::format("Unsupported VM state in snapshot: {}", static_cast<int>(state))};
    if (index < 1)
        throw std::runtime_error{fmt::format("Snapshot index not positive: {}", index)};
    if (index > max_snapshots)
        throw std::runtime_error{fmt::format("Maximum number of snapshots exceeded: {}", index)};
    if (name.empty())
        throw std::runtime_error{"Snapshot names cannot be empty"};
    if (num_cores < 1)
        throw std::runtime_error{
            fmt::format("Invalid number of cores for snapshot: {}", num_cores)};
    if (auto mem_bytes = mem_size.in_bytes(); mem_bytes < 1)
        throw std::runtime_error{fmt::format("Invalid memory size for snapshot: {}", mem_bytes)};
    if (auto disk_bytes = disk_space.in_bytes(); disk_bytes < 1)
        throw std::runtime_error{fmt::format("Invalid disk size for snapshot: {}", disk_bytes)};
}

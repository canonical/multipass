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

#include <multipass/cloud_init_iso.h>
#include <multipass/constants.h>
#include <multipass/json_utils.h>
#include <multipass/virtual_machine_description.h>

#include <fmt/format.h>

namespace mp = multipass;

namespace
{
constexpr auto max_snapshots = 9999;

// When it does not contain cloud_init_instance_id, it signifies that the legacy snapshot does not
// have the item and it needs to fill cloud_init_instance_id with the current value. The current
// value equals to the value at snapshot time because cloud_init_instance_id has been an immutable
// variable up to this point.
std::string choose_cloud_init_instance_id(const boost::json::value* id,
                                          const mp::VirtualMachine& vm)
{
    if (id)
        return value_to<std::string>(*id);

    std::filesystem::path instance_dir{vm.instance_directory().absolutePath().toStdString()};
    return MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(instance_dir /
                                                                  mp::cloud_init_file_name);
}
} // namespace

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
                                             boost::json::object metadata_,
                                             bool upgraded_)
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
      metadata(std::move(metadata_)),
      upgraded(upgraded_)
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

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::SnapshotDescription& desc)
{
    json = {
        {"name", desc.name},
        {"comment", desc.comment},
        {"parent", desc.parent_index},
        {"cloud_init_instance_id", desc.cloud_init_instance_id},
        {"index", desc.index},
        {"creation_timestamp", desc.creation_timestamp.toString(Qt::ISODateWithMs).toStdString()},
        {"num_cores", desc.num_cores},
        {"mem_size", QString::number(desc.mem_size.in_bytes()).toStdString()},
        {"disk_space", QString::number(desc.disk_space.in_bytes()).toStdString()},
        {"extra_interfaces", boost::json::value_from(desc.extra_interfaces)},
        {"state", static_cast<int>(desc.state)},
        {"mounts", boost::json::value_from(desc.mounts, MapAsJsonArray{"target_path"})},
        {"metadata", desc.metadata}};
}

mp::SnapshotDescription mp::tag_invoke(const boost::json::value_to_tag<mp::SnapshotDescription>&,
                                       const boost::json::value& json,
                                       const snapshot_context& ctx)
{
    const auto& json_obj = json.as_object();
    bool upgraded =
        !(json_obj.contains("extra_interfaces") && json_obj.contains("cloud_init_instance_id"));

    return {
        value_to<std::string>(json.at("name")),
        value_to<std::string>(json.at("comment")),
        value_to<int>(json.at("parent")),
        choose_cloud_init_instance_id(json_obj.if_contains("cloud_init_instance_id"), ctx.vm),
        value_to<int>(json.at("index")),
        QDateTime::fromString(value_to<QString>(json.at("creation_timestamp")), Qt::ISODateWithMs),
        value_to<int>(json.at("num_cores")),
        MemorySize{value_to<std::string>(json.at("mem_size"))},
        MemorySize{value_to<std::string>(json.at("disk_space"))},
        lookup_or<std::vector<NetworkInterface>>(json,
                                                 "extra_interfaces",
                                                 std::move(ctx.vm_desc.extra_interfaces)),
        static_cast<mp::VirtualMachine::State>(value_to<int>(json.at("state"))),
        value_to<std::unordered_map<std::string, mp::VMMount>>(json.at("mounts"),
                                                               MapAsJsonArray{"target_path"}),
        json.at("metadata").as_object(),
        upgraded};
}

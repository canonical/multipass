/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hyperv_migration_state.h"

#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/virtual_machine.h>

#include <fmt/format.h>
#include <fmt/std.h>

namespace
{
constexpr auto migration_state_filename = "migration.json";
constexpr auto migration_state_version = 1;

std::string backend_name(multipass::hyperv::HyperVBackend backend)
{
    switch (backend)
    {
    case multipass::hyperv::HyperVBackend::legacy:
        return "legacy";
    case multipass::hyperv::HyperVBackend::hcs:
        return "hcs";
    }

    throw std::runtime_error{"Unknown Hyper-V migration backend"};
}

multipass::hyperv::HyperVBackend parse_backend(const boost::json::value& value)
{
    const auto backend = boost::json::value_to<std::string>(value);
    if (backend == "legacy")
        return multipass::hyperv::HyperVBackend::legacy;
    if (backend == "hcs")
        return multipass::hyperv::HyperVBackend::hcs;

    throw std::runtime_error{fmt::format("Unknown Hyper-V migration backend '{}'", backend)};
}

boost::json::object snapshot_json(const multipass::hyperv::LegacySnapshotDisk& snapshot)
{
    return {
        {"index", snapshot.index},
        {"checkpoint_name", snapshot.checkpoint_name},
        {"checkpoint_id", snapshot.checkpoint_id},
        {"disk_path", snapshot.disk_path.string()},
    };
}

multipass::hyperv::LegacySnapshotDisk parse_snapshot(const boost::json::value& value)
{
    const auto& object = value.as_object();
    return {
        .index = boost::json::value_to<int>(object.at("index")),
        .checkpoint_name = boost::json::value_to<std::string>(object.at("checkpoint_name")),
        .checkpoint_id = boost::json::value_to<std::string>(object.at("checkpoint_id")),
        .disk_path = boost::json::value_to<std::string>(object.at("disk_path")),
    };
}

std::filesystem::path migration_state_path(const std::filesystem::path& instance_dir)
{
    return instance_dir / migration_state_filename;
}
} // namespace

std::optional<multipass::hyperv::HyperVMigrationState>
multipass::hyperv::HyperVMigrationState::load(const std::filesystem::path& instance_dir)
{
    const auto path = migration_state_path(instance_dir);
    const auto contents = MP_FILEOPS.try_read_file(path);
    if (!contents)
        return std::nullopt;

    const auto json = boost::json::parse(*contents);
    const auto& object = json.as_object();
    const auto version = boost::json::value_to<int>(object.at("version"));
    if (version != migration_state_version)
        throw std::runtime_error{
            fmt::format("Unsupported Hyper-V migration state version {}", version)};

    HyperVMigrationState state{
        .backend = parse_backend(object.at("backend")),
        .active_disk = boost::json::value_to<std::string>(object.at("active_disk")),
        .hcs_state_file_stem =
            boost::json::value_to<std::string>(object.at("hcs_state_file_stem")),
    };

    for (const auto& snapshot : object.at("snapshots").as_array())
        state.snapshots.push_back(parse_snapshot(snapshot));

    return state;
}

void multipass::hyperv::HyperVMigrationState::persist(
    const std::filesystem::path& instance_dir) const
{
    boost::json::array snapshot_array;
    snapshot_array.reserve(snapshots.size());
    for (const auto& snapshot : snapshots)
        snapshot_array.push_back(snapshot_json(snapshot));

    const boost::json::object json{
        {"version", migration_state_version},
        {"backend", backend_name(backend)},
        {"active_disk", active_disk.string()},
        {"hcs_state_file_stem", hcs_state_file_stem.string()},
        {"snapshots", std::move(snapshot_array)},
    };

    MP_FILEOPS.write_transactionally(migration_state_path(instance_dir),
                                     multipass::pretty_print(json));
}

void multipass::hyperv::HyperVMigrationState::persist_snapshot_paths(
    const VirtualMachine& vm) const
{
    const auto instance_dir = std::filesystem::path{vm.instance_directory().absolutePath()
                                                       .toStdString()};

    for (const auto& snapshot : snapshots)
    {
        const auto snapshot_path =
            instance_dir / fmt::format("{:04}.snapshot.json", snapshot.index);
        const auto contents = MP_FILEOPS.try_read_file(snapshot_path);
        if (!contents)
            throw std::runtime_error{
                fmt::format("Could not read snapshot metadata '{}'", snapshot_path)};

        auto json = boost::json::parse(*contents);
        auto& snapshot_object = json.at("snapshot").as_object();
        if (boost::json::value_to<int>(snapshot_object.at("index")) != snapshot.index)
            throw std::runtime_error{
                fmt::format("Snapshot metadata index does not match '{}'", snapshot_path)};

        snapshot_object["disk_path"] = snapshot.disk_path.string();
        MP_FILEOPS.write_transactionally(snapshot_path, multipass::pretty_print(json));
    }
}

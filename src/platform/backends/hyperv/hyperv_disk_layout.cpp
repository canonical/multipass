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

#include "hyperv_disk_layout.h"

#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/snapshot.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>
#include <shared/windows/powershell.h>

#include <fmt/format.h>
#include <fmt/std.h>

#include <algorithm>
#include <map>

namespace
{
namespace fs = std::filesystem;
namespace mhv = multipass::hyperv;
using multipass::hyperv::virtdisk::VirtDisk;

QString quoted_name(const std::string& name)
{
    auto escaped = QString::fromStdString(name);
    escaped.replace('\'', "''");
    return "'" + escaped + "'";
}

std::vector<fs::path> disk_chain(const fs::path& disk)
{
    std::vector<fs::path> chain;
    if (const auto result = VirtDisk().list_virtual_disk_chain(disk, chain); !result)
        throw std::runtime_error{
            fmt::format("Could not inspect virtual disk chain for '{}': {}", disk, result)};

    if (chain.empty())
        throw std::runtime_error{fmt::format("Virtual disk chain for '{}' is empty", disk)};

    return chain;
}

bool same_path(const fs::path& lhs, const fs::path& rhs)
{
    return MP_FILEOPS.weakly_canonical(lhs) == MP_FILEOPS.weakly_canonical(rhs);
}

void append_unique(std::vector<fs::path>& disks, const std::vector<fs::path>& chain)
{
    for (const auto& disk : chain)
    {
        if (!MP_FILEOPS.exists(disk))
            throw std::runtime_error{fmt::format("Virtual disk '{}' does not exist", disk)};

        if (std::ranges::none_of(disks, [&disk](const auto& existing) {
                return same_path(existing, disk);
            }))
            disks.push_back(disk);
    }
}

struct DiscoveredVM
{
    fs::path active_disk;
    std::map<std::string, mhv::LegacySnapshotDisk> snapshots;
};

DiscoveredVM query_hyperv(const std::string& name)
{
    multipass::PowerShell powershell{name};
    const auto vm_name = quoted_name(name);
    const auto script = QStringLiteral(
                            "$vmName=%1; "
                            "$primary=@(Get-VMHardDiskDrive -VMName $vmName -ErrorAction Stop | "
                            "Where-Object {$_.ControllerType -eq 'SCSI' -and "
                            "$_.ControllerNumber -eq 0 -and $_.ControllerLocation -eq 0}); "
                            "if ($primary.Count -ne 1) { throw 'Expected one primary disk' }; "
                            "$checkpoints=@(Get-VMCheckpoint -VMName $vmName -ErrorAction Stop | "
                            "ForEach-Object { "
                            "$disk=@(Get-VMHardDiskDrive -VMSnapshot $_ | "
                            "Where-Object {$_.ControllerType -eq 'SCSI' -and "
                            "$_.ControllerNumber -eq 0 -and $_.ControllerLocation -eq 0}); "
                            "if ($disk.Count -ne 1) { throw 'Expected one checkpoint disk' }; "
                            "[PSCustomObject]@{Name=$_.Name;Id=$_.Id.ToString();Path=$disk[0].Path} "
                            "}); "
                            "[PSCustomObject]@{ActiveDisk=$primary[0].Path;"
                            "Snapshots=$checkpoints} | ConvertTo-Json -Compress -Depth 4")
                            .arg(vm_name);

    QString output;
    QString output_error;
    if (!powershell.run({script}, &output, &output_error))
        throw std::runtime_error{
            fmt::format("Could not inspect Hyper-V disk layout for '{}': {}",
                        name,
                        output_error.toStdString())};

    const auto json = boost::json::parse(output.toStdString());
    const auto& object = json.as_object();
    DiscoveredVM discovered{
        .active_disk = boost::json::value_to<std::string>(object.at("ActiveDisk"))};

    for (const auto& value : object.at("Snapshots").as_array())
    {
        const auto& snapshot = value.as_object();
        mhv::LegacySnapshotDisk disk{
            .index = 0,
            .checkpoint_name = boost::json::value_to<std::string>(snapshot.at("Name")),
            .checkpoint_id = boost::json::value_to<std::string>(snapshot.at("Id")),
            .disk_path = boost::json::value_to<std::string>(snapshot.at("Path")),
        };

        const auto [_, inserted] =
            discovered.snapshots.emplace(disk.checkpoint_name, std::move(disk));
        if (!inserted)
            throw std::runtime_error{"Hyper-V contains duplicate checkpoint names"};
    }

    return discovered;
}
} // namespace

bool multipass::hyperv::HyperVDiskLayoutResolver::vm_exists(const std::string& name)
{
    PowerShell powershell{name};
    QString output;
    return powershell.run(
               {QStringLiteral("if (Get-VM -Name %1 -ErrorAction SilentlyContinue) "
                               "{ 'true' } else { 'false' }")
                    .arg(quoted_name(name))},
               &output,
               nullptr,
               true) &&
           output == "true";
}

std::filesystem::path
multipass::hyperv::HyperVDiskLayoutResolver::active_disk(const std::string& name)
{
    return query_hyperv(name).active_disk;
}

multipass::hyperv::LegacyHyperVDiskLayout
multipass::hyperv::HyperVDiskLayoutResolver::resolve(const std::string& name,
                                                     const VirtualMachine& vm)
{
    auto discovered = query_hyperv(name);
    if (discovered.active_disk.empty())
        throw std::runtime_error{"Hyper-V returned an empty active disk path"};

    LegacyHyperVDiskLayout layout{.active_disk = std::move(discovered.active_disk)};
    const auto multipass_snapshots = vm.view_snapshots();
    if (multipass_snapshots.size() != discovered.snapshots.size())
        throw std::runtime_error{
            fmt::format("Hyper-V checkpoint count ({}) does not match Multipass snapshot count ({})",
                        discovered.snapshots.size(),
                        multipass_snapshots.size())};

    std::map<int, fs::path> snapshot_paths;
    std::vector<fs::path> unique_snapshot_paths;
    for (const auto& snapshot : multipass_snapshots)
    {
        const auto expected_name = fmt::format("@s{}", snapshot->get_index());
        auto checkpoint = discovered.snapshots.find(expected_name);
        if (checkpoint == discovered.snapshots.end())
            throw std::runtime_error{
                fmt::format("Could not find Hyper-V checkpoint '{}'", expected_name)};

        checkpoint->second.index = snapshot->get_index();
        if (std::ranges::any_of(unique_snapshot_paths,
                                [&checkpoint](const auto& path) {
                                    return same_path(path, checkpoint->second.disk_path);
                                }))
            throw std::runtime_error{
                fmt::format("Multiple Hyper-V checkpoints reference disk '{}'",
                            checkpoint->second.disk_path)};

        unique_snapshot_paths.push_back(checkpoint->second.disk_path);
        snapshot_paths.emplace(snapshot->get_index(), checkpoint->second.disk_path);
        layout.snapshots.push_back(std::move(checkpoint->second));
        discovered.snapshots.erase(checkpoint);
    }

    if (!discovered.snapshots.empty())
        throw std::runtime_error{"Hyper-V contains checkpoints not managed by Multipass"};

    const auto active_chain = disk_chain(layout.active_disk);
    append_unique(layout.all_disks, active_chain);
    const auto& expected_base = active_chain.back();

    for (const auto& snapshot : multipass_snapshots)
    {
        const auto& path = snapshot_paths.at(snapshot->get_index());
        const auto chain = disk_chain(path);
        append_unique(layout.all_disks, chain);
        if (!same_path(chain.back(), expected_base))
            throw std::runtime_error{
                fmt::format("Snapshot '{}' does not share the active disk's base image",
                            snapshot->get_name())};

        if (const auto parent = snapshot->get_parent())
        {
            const auto& parent_path = snapshot_paths.at(parent->get_index());
            if (chain.size() < 2 || !same_path(chain[1], parent_path))
                throw std::runtime_error{
                    fmt::format("Snapshot '{}' is not directly attached to its Multipass parent",
                                snapshot->get_name())};
        }
    }

    const auto head_path =
        std::filesystem::path{vm.instance_directory().absolutePath().toStdString()} /
        "snapshot-head";
    if (const auto contents = MP_FILEOPS.try_read_file(head_path))
    {
        const auto head_index = std::stoi(utils::trim(std::string{*contents}));
        if (head_index != 0)
        {
            const auto snapshot = snapshot_paths.find(head_index);
            if (snapshot == snapshot_paths.end())
                throw std::runtime_error{
                    fmt::format("Snapshot head {} has no corresponding snapshot", head_index)};

            if (active_chain.size() < 2 || !same_path(active_chain[1], snapshot->second))
                throw std::runtime_error{
                    fmt::format("Active disk is not directly attached to snapshot head {}",
                                head_index)};
        }
    }
    else if (!multipass_snapshots.empty())
    {
        throw std::runtime_error{"Multipass snapshot head metadata is missing"};
    }

    std::ranges::sort(layout.snapshots, {}, &LegacySnapshotDisk::index);
    return layout;
}

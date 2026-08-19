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

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace multipass
{
class VirtualMachine;
}

namespace multipass::hyperv
{
enum class HyperVBackend
{
    legacy,
    hcs,
};

struct LegacySnapshotDisk
{
    int index;
    std::string checkpoint_name;
    std::string checkpoint_id;
    std::filesystem::path disk_path;
};

struct HyperVMigrationState
{
    HyperVBackend backend{HyperVBackend::legacy};
    std::filesystem::path active_disk;
    std::filesystem::path hcs_state_file_stem;
    std::vector<LegacySnapshotDisk> snapshots;

    static std::optional<HyperVMigrationState> load(const std::filesystem::path& instance_dir);

    void persist(const std::filesystem::path& instance_dir) const;
    void persist_snapshot_paths(const VirtualMachine& vm) const;
};
} // namespace multipass::hyperv

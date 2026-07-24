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

#include <filesystem>
#include <vector>

namespace multipass::hyperv::virtdisk
{

using PathPairs = std::vector<std::pair<std::filesystem::path, std::filesystem::path>>;

/**
 * Rebuild a deleted snapshot's direct children transactionally.
 *
 * Each child is rebuilt from the deleted snapshot. Rebuilt disks get new VHDX
 * identities, so grandchildren must be reparented to the rebuilt files.
 *
 * Rollback restores the original files and parent links.
 */
class ChildRebuild
{
public:
    ChildRebuild(std::filesystem::path self_path,
                 std::vector<std::filesystem::path> children,
                 PathPairs grandchildren,
                 std::string log_category);
    // Move self aside before arming rollback.
    void begin();

    // Merge each child into a fresh copy of self, staged as "<child>.new".
    void stage();

    // Swap staged children into place, keeping originals for rollback.
    void commit();

    // Refresh grandchildren after their rebuilt parents get new VHDX identities.
    void reparent();

    // Drop unreferenced backups.
    void finalize() noexcept;

    // Restore the pre-erase state without masking the original error.
    void rollback() noexcept;

private:
    std::filesystem::path self_path;
    std::filesystem::path self_backup;
    std::vector<std::filesystem::path> children;

    PathPairs grandchildren{};
    PathPairs staged{};
    PathPairs reparented{};

    std::string log_category;
};

} // namespace multipass::hyperv::virtdisk

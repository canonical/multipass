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

#include "hyperv_migration_state.h"

#include <filesystem>
#include <string>
#include <vector>

namespace multipass
{
class VirtualMachine;
}

namespace multipass::hyperv
{
struct LegacyHyperVDiskLayout
{
    std::filesystem::path active_disk;
    std::vector<LegacySnapshotDisk> snapshots;
    std::vector<std::filesystem::path> all_disks;
};

class HyperVDiskLayoutResolver
{
public:
    [[nodiscard]] static bool vm_exists(const std::string& name);
    [[nodiscard]] static std::filesystem::path active_disk(const std::string& name);
    [[nodiscard]] static LegacyHyperVDiskLayout resolve(const std::string& name,
                                                        const VirtualMachine& vm);
};
} // namespace multipass::hyperv

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

#include <hyperv_api/virtdisk/virtdisk_child_rebuild.h>

#include <hyperv_api/virtdisk/virtdisk_exceptions.h>
#include <hyperv_api/virtdisk/virtdisk_utils.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/file_ops.h>
#include <multipass/logging/log.h>

namespace
{
namespace mpl = multipass::logging;

// Keep backup/scratch files beside their disk.
std::filesystem::path with_suffix(std::filesystem::path p, const char* suffix)
{
    p += suffix;
    return p;
}
} // namespace

namespace multipass::hyperv::virtdisk
{

ChildRebuild::ChildRebuild(std::filesystem::path self_path,
                           std::vector<std::filesystem::path> children,
                           PathPairs grandchildren,
                           std::string log_category)
    : self_path{std::move(self_path)},
      self_backup{with_suffix(this->self_path, ".tmp")},
      children{std::move(children)},
      grandchildren{std::move(grandchildren)},
      log_category{log_category}
{
    staged.reserve(this->children.size());
}

// Move self aside before arming rollback.
void ChildRebuild::begin()
{
    MP_FILEOPS.rename(self_path, self_backup);
}

// Merge each child into a fresh copy of self, staged as "<child>.new".
void ChildRebuild::stage()
{
    for (const auto& child_path : children)
    {
        auto staged_path = with_suffix(child_path, ".new");

        MP_FILEOPS.copy(self_backup, self_path, {});

        if (auto merge_r = VirtDisk().merge_virtual_disk_into_parent(child_path); !merge_r)
            throw CreateVirtdiskSnapshotError{merge_r,
                                              "Could not merge child disk `{}` into `{}`",
                                              child_path,
                                              self_path};

        MP_FILEOPS.rename(self_path, staged_path);
        staged.emplace_back(child_path, staged_path);
    }
}

// Swap staged children into place, keeping originals for rollback.
void ChildRebuild::commit()
{
    for (const auto& [child_path, staged_path] : staged)
    {
        MP_FILEOPS.rename(child_path, with_suffix(child_path, ".old"));
        MP_FILEOPS.rename(staged_path, child_path);
    }
}

// Refresh grandchildren after their rebuilt parents get new VHDX identities.
void ChildRebuild::reparent()
{
    for (const auto& [grandchild, child_path] : grandchildren)
    {
        if (const auto r = VirtDisk().reparent_virtual_disk(grandchild, child_path); !r)
            throw CreateVirtdiskSnapshotError{r,
                                              "Could not reparent `{}` onto rebuilt `{}`",
                                              grandchild,
                                              child_path};
        reparented.emplace_back(grandchild, child_path);
    }
}

// Drop unreferenced backups.
void ChildRebuild::finalize() noexcept
{
    std::error_code ec{};
    for (const auto& [child_path, staged_path] : staged)
        MP_FILEOPS.remove(with_suffix(child_path, ".old"), ec);
    MP_FILEOPS.remove(self_backup, ec);
}

// Restore the pre-erase state without masking the original error.
void ChildRebuild::rollback() noexcept
{
    std::error_code ec{};
    for (const auto& [child_path, staged_path] : staged)
    {
        const auto backup_path = with_suffix(child_path, ".old");
        if (MP_FILEOPS.exists(backup_path, ec))
        {
            MP_FILEOPS.remove(child_path, ec);
            try_rename(log_category, backup_path, child_path);
        }
        MP_FILEOPS.remove(staged_path, ec);
    }
    MP_FILEOPS.remove(self_path, ec);
    try_rename(log_category, self_backup, self_path);

    // Restore already-updated parent links.
    for (const auto& [grandchild, child_path] : reparented)
        if (const auto r = VirtDisk().reparent_virtual_disk(grandchild, child_path); !r)
            mpl::error(log_category,
                       "Failed to restore parent linkage of `{}` onto `{}` during rollback",
                       grandchild,
                       child_path);
}

} // namespace multipass::hyperv::virtdisk

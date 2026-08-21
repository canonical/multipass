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

#include <string_view>

namespace
{
namespace mpl = multipass::logging;

// Keep backup/scratch files beside their disk.
std::filesystem::path with_suffix(std::filesystem::path p, const char* suffix)
{
    p += suffix;
    return p;
}

void logged_remove(std::string_view log_category, const std::filesystem::path& path) noexcept
{
    std::error_code ec;
    MP_FILEOPS.remove(path, ec);
    if (ec)
        mpl::error(log_category, "Failed to remove `{}`: {}", path, ec.message());
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

void ChildRebuild::begin()
{
    MP_FILEOPS.rename(self_path, self_backup);
}

void ChildRebuild::stage()
{
    for (const auto& child_path : children)
    {
        auto staged_path = with_suffix(child_path, ".new");

        MP_FILEOPS.copy(self_backup, self_path, {});

        if (auto merge_r = VirtDisk().merge_virtual_disk_into_parent(child_path); !merge_r)
            throw VirtdiskSnapshotError{merge_r,
                                        "Could not merge child disk `{}` into `{}`",
                                        child_path,
                                        self_path};

        MP_FILEOPS.rename(self_path, staged_path);
        staged.emplace_back(child_path, staged_path);
    }
}

void ChildRebuild::commit()
{
    for (const auto& [child_path, staged_path] : staged)
    {
        MP_FILEOPS.rename(child_path, with_suffix(child_path, ".old"));
        MP_FILEOPS.rename(staged_path, child_path);
    }
}

void ChildRebuild::reparent()
{
    for (const auto& [grandchild, child_path] : grandchildren)
    {
        if (const auto r = VirtDisk().reparent_virtual_disk(grandchild, child_path); !r)
            throw VirtdiskSnapshotError{r,
                                        "Could not reparent `{}` onto rebuilt `{}`",
                                        grandchild,
                                        child_path};
        reparented.emplace_back(grandchild, child_path);
    }
}

void ChildRebuild::finalize() noexcept
{
    for (const auto& paths : staged)
        logged_remove(log_category, with_suffix(paths.first, ".old"));
    logged_remove(log_category, self_backup);
}

void ChildRebuild::rollback() noexcept
{
    for (const auto& [child_path, staged_path] : staged)
    {
        const auto backup_path = with_suffix(child_path, ".old");
        std::error_code ec;
        if (MP_FILEOPS.exists(backup_path, ec))
        {
            logged_remove(log_category, child_path);
            try_rename(log_category, backup_path, child_path);
        }
        else if (ec)
            mpl::error(log_category,
                       "Failed to inspect backup `{}`: {}",
                       backup_path,
                       ec.message());

        logged_remove(log_category, staged_path);
    }
    logged_remove(log_category, self_path);
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

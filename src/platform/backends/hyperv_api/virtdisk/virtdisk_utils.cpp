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

#include <hyperv_api/virtdisk/virtdisk_utils.h>

#include <hyperv_api/virtdisk/virtdisk_exceptions.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/file_ops.h>
#include <multipass/top_catch_all.h>

namespace multipass::hyperv::virtdisk
{

void try_rename(std::string_view log_category,
                const std::filesystem::path& from,
                const std::filesystem::path& to) noexcept
{
    top_catch_all(log_category, [from, to] { MP_FILEOPS.rename(from, to); });
}

bool is_direct_child_of(const std::filesystem::path& disk, const std::filesystem::path& parent_disk)
{
    if (!MP_FILEOPS.exists(disk) || !MP_FILEOPS.exists(parent_disk))
        return false;

    std::vector<std::filesystem::path> chain;
    if (const auto r = VirtDisk().list_virtual_disk_chain(disk, chain, 2); !r)
    {
        throw CreateVirtdiskSnapshotError{r, "Could not inspect virtual disk chain for `{}`", disk};
    }

    return chain.size() >= 2 &&
           MP_FILEOPS.weakly_canonical(chain[1]) == MP_FILEOPS.weakly_canonical(parent_disk);
}

} // namespace multipass::hyperv::virtdisk

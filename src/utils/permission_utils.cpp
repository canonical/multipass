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

#include <multipass/utils/permission_utils.h>

#include <multipass/file_ops.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace fs = mp::fs;

namespace
{
void set_single_permissions(const fs::path& path, const fs::perms& permissions, bool try_inherit)
{
    if (!MP_PLATFORM.set_permissions(path, permissions, try_inherit))
        throw std::runtime_error(fmt::format("Cannot set permissions for '{}'", path.string()));
}

void set_single_owner(const fs::path& path)
{
    if (!MP_PLATFORM.take_ownership(path))
        throw std::runtime_error(fmt::format("Cannot set owner for '{}'", path.string()));
}

// only exists because MP_FILEOPS doesn't overload the throwing variations of std::filesystem functions
void throw_if_error(const fs::path& path, const std::error_code& ec)
{
    if (ec)
        throw std::system_error(
            ec,
            fmt::format("System error occurred while handling permissions for '{}'", path.string()));
}

// recursively iterates over all files in folder and applies a function that takes a path
template <class Func>
void apply_on_files(const fs::path& path, Func&& func)
{
    std::error_code ec{};
    if (!MP_FILEOPS.exists(path, ec) || ec)
        throw std::runtime_error(fmt::format("Cannot handle permissions for nonexistent file '{}'", path.string()));

    func(path, true);

    // iterate over children of directory
    if (MP_FILEOPS.is_directory(path, ec))
    {
        const auto dir_iterator = MP_FILEOPS.recursive_dir_iterator(path, ec);
        throw_if_error(path, ec);

        if (!dir_iterator) [[unlikely]]
            throw std::runtime_error(fmt::format("Cannot iterate over directory '{}'", path.string()));

        while (dir_iterator->hasNext())
        {
            const auto& entry = dir_iterator->next();

            func(entry.path(), false);
        }
    }

    throw_if_error(path, ec);
}
} // namespace

mp::PermissionUtils::PermissionUtils(const PrivatePass& pass) noexcept : Singleton{pass}
{
}

void mp::PermissionUtils::restrict_permissions(const fs::path& path) const
{
    apply_on_files(path, [&](const fs::path& apply_path, bool root_dir) {
        set_single_owner(apply_path);
        set_single_permissions(apply_path, fs::perms::owner_all, !root_dir);
    });
}

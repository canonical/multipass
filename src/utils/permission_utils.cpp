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

using Path = mp::PermissionUtils::Path;

namespace
{
void set_single_permissions(const Path& path, const QFileDevice::Permissions& permissions)
{
    QString qpath = QString::fromUtf8(path.u8string());

    if (!MP_PLATFORM.set_permissions(qpath, permissions))
        throw std::runtime_error(fmt::format("Cannot set permissions for '{}'", path.string()));
}

void set_single_owner(const Path& path, bool root)
{
    QString qpath = QString::fromUtf8(path.u8string());

    if (!MP_PLATFORM.take_ownership(qpath, root))
        throw std::runtime_error(fmt::format("Cannot set owner for '{}'", path.string()));
}

// only exists because MP_FILEOPS doesn't overload the throwing variaions of std::filesystem functions
void throw_if_error(const Path& path, const std::error_code& ec)
{
    if (ec)
        throw std::system_error(
            ec,
            fmt::format("System error occurred while handling permissions for '{}'", path.string()));
}

// recursively iterates over all files in folder and applies a function that takes a path
template <class Func>
void apply_on_files(const Path& path, Func&& func)
{
    std::error_code ec{};
    if (!MP_FILEOPS.exists(path, ec) || ec)
        throw std::runtime_error(fmt::format("Cannot handle permissions for nonexistent file '{}'", path.string()));

    func(path);

    // iterate over children if directory
    if (MP_FILEOPS.is_directory(path, ec))
    {
        auto dir_iterator = MP_FILEOPS.recursive_dir_iterator(path, ec);
        throw_if_error(path, ec);

        if (!dir_iterator) [[unlikely]]
            throw std::runtime_error(fmt::format("Cannot iterate over directory '{}'", path.string()));

        while (dir_iterator->hasNext())
        {
            const auto& entry = dir_iterator->next();

            func(entry.path());
        }
    }

    throw_if_error(path, ec);
}
} // namespace

mp::PermissionUtils::PermissionUtils(const Singleton<PermissionUtils>::PrivatePass& pass) noexcept
    : Singleton<PermissionUtils>::Singleton{pass}
{
}

void mp::PermissionUtils::set_permissions(const Path& path, const QFileDevice::Permissions& permissions) const
{
    apply_on_files(path, [&](const fs::path& apply_path) { set_single_permissions(apply_path, permissions); });
}

void mp::PermissionUtils::take_ownership(const Path& path, bool root) const
{
    apply_on_files(path, [&](const fs::path& apply_path) { set_single_owner(apply_path, root); });
}

void mp::PermissionUtils::restrict_permissions(const Path& path) const
{
    apply_on_files(path, [&](const fs::path& apply_path) {
        set_single_owner(apply_path, true);
        set_single_permissions(apply_path, QFile::ReadOwner | QFile::WriteOwner);
    });
}

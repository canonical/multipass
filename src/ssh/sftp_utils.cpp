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

#include <multipass/file_ops.h>
#include <multipass/ssh/sftp_utils.h>

#include <fmt/std.h>

namespace multipass
{
SFTPUtils::SFTPUtils(const PrivatePass& pass) noexcept : Singleton<SFTPUtils>::Singleton{pass}
{
}

fs::path SFTPUtils::get_local_file_target(const fs::path& source_path, const fs::path& target_path, bool make_parent)
{
    std::error_code err;
    if (!MP_FILEOPS.exists(target_path, err) && !err)
    {
        auto parent_path = target_path.parent_path();
        parent_path = parent_path.empty() ? "." : parent_path;
        if (make_parent && !MP_FILEOPS.create_directories(parent_path, err) && err)
            throw SFTPError{"cannot create local directory {}: {}", parent_path, err.message()};
        if (MP_FILEOPS.exists(parent_path, err) && !err)
            return target_path;
        else if (err)
            throw SFTPError{"cannot access {}: {}", parent_path, err.message()};
        else
            throw SFTPError{"local target does not exist"};
    }
    else if (err)
        throw SFTPError{"cannot access {}: {}", target_path, err.message()};

    if (!MP_FILEOPS.is_directory(target_path, err))
        return target_path;

    auto target_full_path = target_path / source_path.filename();
    if (MP_FILEOPS.is_directory(target_full_path, err))
        throw SFTPError{"cannot overwrite local directory {} with non-directory", target_full_path};
    else if (err && err != std::errc::no_such_file_or_directory)
        throw SFTPError{"cannot access {}: {}", target_full_path, err.message()};

    return target_full_path;
}

fs::path SFTPUtils::get_remote_file_target(sftp_session sftp, const fs::path& source_path, const fs::path& target_path,
                                           bool make_parent)
{
    auto target_full_path = target_path.empty() ? source_path.filename() : target_path;

    auto target_attr = mp_sftp_stat(sftp, target_full_path.u8string().c_str());
    if (!target_attr)
    {
        auto parent_path = target_full_path.parent_path().u8string();
        parent_path = parent_path.empty() ? "." : parent_path;
        if (make_parent)
            mkdir_recursive(sftp, parent_path);
        return mp_sftp_stat(sftp, parent_path.c_str()) ? target_full_path
                                                       : throw SFTPError{"remote target does not exist"};
    }

    if (target_attr->type != SSH_FILEXFER_TYPE_DIRECTORY)
        return target_full_path;

    target_full_path += source_path.filename().u8string().insert(0, "/");
    target_attr = mp_sftp_stat(sftp, target_full_path.u8string().c_str());
    if (target_attr && target_attr->type == SSH_FILEXFER_TYPE_DIRECTORY)
        throw SFTPError{"cannot overwrite remote directory {:?} with non-directory", target_full_path};

    return target_full_path;
}

fs::path SFTPUtils::get_local_dir_target(const fs::path& source_path, const fs::path& target_path, bool make_parent)
{
    std::error_code err;

    if (MP_FILEOPS.exists(target_path, err) && !MP_FILEOPS.is_directory(target_path, err) && !err)
        throw SFTPError{"cannot overwrite local non-directory {} with directory", target_path};
    else if (err)
        throw SFTPError{"cannot access {}: {}", target_path, err.message()};

    if (!MP_FILEOPS.exists(target_path, err))
    {
        if (make_parent ? MP_FILEOPS.create_directories(target_path, err)
                        : MP_FILEOPS.create_directory(target_path, err);
            err)
            throw SFTPError{"cannot create local directory {}: {}", target_path, err.message()};
        return target_path;
    }

    auto child_path = target_path / source_path.filename();
    if (MP_FILEOPS.exists(child_path, err) && !MP_FILEOPS.is_directory(child_path, err) && !err)
        throw SFTPError{"cannot overwrite local non-directory {} with directory", child_path};
    else if (err)
        throw SFTPError{"cannot access {}: {}", child_path, err.message()};

    if (!MP_FILEOPS.create_directory(child_path, err) && err)
        throw SFTPError{"cannot create local directory {}: {}", child_path, err.message()};

    return child_path;
}

fs::path SFTPUtils::get_remote_dir_target(sftp_session sftp, const fs::path& source_path, const fs::path& target_path,
                                          bool make_parent)
{
    auto target_path_string = target_path.u8string();
    auto target_info = mp_sftp_stat(sftp, target_path_string.c_str());

    if (target_info && target_info->type != SSH_FILEXFER_TYPE_DIRECTORY)
        throw SFTPError{"cannot overwrite remote non-directory {} with directory", target_path};

    if (!target_info)
    {
        if (make_parent)
            mkdir_recursive(sftp, target_path);
        else if (sftp_mkdir(sftp, target_path_string.c_str(), 0777) != SSH_FX_OK)
            throw SFTPError{"cannot create remote directory {}: {}", target_path, ssh_get_error(sftp->session)};
        return target_path;
    }

    fs::path child_path = target_path_string + '/' + source_path.filename().u8string();
    auto child_path_string = child_path.u8string();
    auto child_info = mp_sftp_stat(sftp, child_path_string.c_str());
    if (child_info && child_info->type != SSH_FILEXFER_TYPE_DIRECTORY)
        throw SFTPError{"cannot overwrite remote non-directory {:?} with directory", child_path};

    if (!child_info && sftp_mkdir(sftp, child_path_string.c_str(), 0777) != SSH_FX_OK)
        throw SFTPError{"cannot create remote directory {:?}: {}", child_path, ssh_get_error(sftp->session)};

    return child_path;
}

void SFTPUtils::mkdir_recursive(sftp_session sftp, const fs::path& path)
{
    std::vector<fs::path> partial_paths;
    // this takes a path and creates a list of all sub-paths
    // e.g "some/nested/path" => ["some", "some/nested", "some/nested/path"]
    std::partial_sum(path.begin(), path.end(), std::back_inserter(partial_paths),
                     [](auto acc, auto curr) { return acc.u8string() + '/' + curr.u8string(); });
    for (const auto& partial_path : partial_paths)
        if (auto attr = mp_sftp_lstat(sftp, partial_path.u8string().c_str());
            attr && attr->type != SSH_FILEXFER_TYPE_DIRECTORY)
            throw SFTPError{"cannot overwrite remote non-directory {:?} with directory", partial_path};
        else if (!attr && sftp_mkdir(sftp, partial_path.u8string().c_str(), 0777) != SSH_FX_OK)
            throw SFTPError{"cannot create remote directory {:?}: {}", partial_path, ssh_get_error(sftp->session)};
}

std::unique_ptr<SFTPClient> SFTPUtils::make_SFTPClient(const std::string& host, int port, const std::string& username,
                                                       const std::string& priv_key_blob)
{
    return std::make_unique<SFTPClient>(host, port, username, priv_key_blob);
}

std::unique_ptr<SFTPDirIterator> SFTPUtils::make_SFTPDirIterator(sftp_session sftp, const fs::path& path)
{
    return std::make_unique<SFTPDirIterator>(sftp, path);
}

} // namespace multipass

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

#ifndef MULTIPASS_SFTP_UTILS_H
#define MULTIPASS_SFTP_UTILS_H

#include "sftp_client.h"
#include "sftp_dir_iterator.h"

#include <multipass/format.h>
#include <multipass/singleton.h>

#define MP_SFTP_UNIQUE_PTR(open, close)                                                                                \
    template <typename... Args>                                                                                        \
    auto mp_##open(Args&&... args)                                                                                     \
    {                                                                                                                  \
        return std::unique_ptr<std::remove_pointer_t<decltype(std::function{open})::result_type>,                      \
                               decltype(std::function{close})>{open(std::forward<Args>(args)...), close};              \
    }

namespace multipass
{
struct SFTPError : public std::runtime_error
{
    template <typename... Args>
    explicit SFTPError(const char* fmt, Args&&... args) : runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {
    }
};

MP_SFTP_UNIQUE_PTR(sftp_new, sftp_free)
MP_SFTP_UNIQUE_PTR(sftp_open, sftp_close)
MP_SFTP_UNIQUE_PTR(sftp_stat, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_lstat, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_opendir, sftp_closedir)
MP_SFTP_UNIQUE_PTR(sftp_readdir, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_readlink, free)

#define MP_SFTPUTILS multipass::SFTPUtils::instance()

struct SFTPUtils : public Singleton<SFTPUtils>
{
    SFTPUtils(const Singleton<SFTPUtils>::PrivatePass&) noexcept;

    virtual fs::path get_local_file_target(const fs::path& source_path, const fs::path& target_path, bool make_parent);
    virtual fs::path get_remote_file_target(sftp_session sftp, const fs::path& source_path, const fs::path& target_path,
                                            bool make_parent);
    virtual fs::path get_local_dir_target(const fs::path& source_path, const fs::path& target_path, bool make_parent);
    virtual fs::path get_remote_dir_target(sftp_session sftp, const fs::path& source_path, const fs::path& target_path,
                                           bool make_parent);
    virtual void mkdir_recursive(sftp_session sftp, const fs::path& path);
    virtual std::unique_ptr<SFTPDirIterator> make_SFTPDirIterator(sftp_session sftp, const fs::path& path);
    virtual std::unique_ptr<SFTPClient> make_SFTPClient(const std::string& host, int port, const std::string& username,
                                                        const std::string& priv_key_blob);
};
} // namespace multipass

#endif

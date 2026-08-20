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

#include "sftp_client.h"
#include "sftp_dir_iterator.h"

#include <multipass/format.h>
#include <multipass/singleton.h>
#include <multipass/ssh/libssh_wrapper.h>

#include <functional>

#define MP_SFTP_UNIQUE_PTR_DELETER_true(close) [](auto* ptr) { return MP_LIBSSH.close(ptr); }
#define MP_SFTP_UNIQUE_PTR_DELETER_false(close) close
#define MP_SFTP_UNIQUE_PTR_AUX(open, close, wrap)                                                  \
    template <typename... Args>                                                                    \
    auto mp_##open(Args&&... args)                                                                 \
    {                                                                                              \
        return std::unique_ptr<std::remove_pointer_t<decltype(std::function{open})::result_type>,  \
                               decltype(std::function{close})>{                                    \
            MP_LIBSSH.open(std::forward<Args>(args)...),                                           \
            MP_SFTP_UNIQUE_PTR_DELETER_##wrap(close)};                                             \
    }

#define MP_SFTP_UNIQUE_PTR(open, close) MP_SFTP_UNIQUE_PTR_AUX(open, close, true)
#define MP_SFTP_UNIQUE_PTR_RAW(open, close) MP_SFTP_UNIQUE_PTR_AUX(open, close, false)

namespace multipass
{
struct SFTPError : public std::runtime_error
{
    template <typename... Args>
    explicit SFTPError(fmt::format_string<Args...> fmt, Args&&... args)
        : runtime_error(fmt::format(fmt, std::forward<Args>(args)...))
    {
    }
};

MP_SFTP_UNIQUE_PTR(sftp_new, sftp_free)
MP_SFTP_UNIQUE_PTR(sftp_open, sftp_close)
MP_SFTP_UNIQUE_PTR(sftp_limits, sftp_limits_free)
MP_SFTP_UNIQUE_PTR(sftp_stat, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_lstat, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_opendir, sftp_closedir)
MP_SFTP_UNIQUE_PTR(sftp_readdir, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR_RAW(sftp_readlink, free)

#define MP_SFTPUTILS multipass::SFTPUtils::instance()

struct SFTPUtils : public Singleton<SFTPUtils>
{
    SFTPUtils(const Singleton<SFTPUtils>::PrivatePass&) noexcept;

    virtual fs::path get_local_file_target(const fs::path& source_path,
                                           const fs::path& target_path,
                                           bool make_parent);
    virtual fs::path get_remote_file_target(sftp_session sftp,
                                            const fs::path& source_path,
                                            const fs::path& target_path,
                                            bool make_parent);
    virtual fs::path get_local_dir_target(const fs::path& source_path,
                                          const fs::path& target_path,
                                          bool make_parent);
    virtual fs::path get_remote_dir_target(sftp_session sftp,
                                           const fs::path& source_path,
                                           const fs::path& target_path,
                                           bool make_parent);
    virtual void mkdir_recursive(sftp_session sftp, const fs::path& path);
    virtual std::unique_ptr<SFTPDirIterator> make_SFTPDirIterator(sftp_session sftp,
                                                                  const fs::path& path);
    virtual std::unique_ptr<SFTPClient> make_SFTPClient(const SSHCoordinates& coordinates);
};
} // namespace multipass

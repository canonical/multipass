/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#include <functional>
#include <libssh/sftp.h>
#include <memory>
#include <stack>

namespace
{
#define MP_SFTP_UNIQUE_PTR(open, close)                                                                                \
    template <typename... Args>                                                                                        \
    auto mp_##open(Args&&... args)                                                                                     \
    {                                                                                                                  \
        return std::unique_ptr<std::remove_pointer_t<decltype(std::function{open})::result_type>,                      \
                               decltype(std::function{close})>{open(std::forward<Args>(args)...), close};              \
    }
} // namespace

namespace multipass
{
MP_SFTP_UNIQUE_PTR(sftp_new, sftp_free)
MP_SFTP_UNIQUE_PTR(sftp_open, sftp_close)
MP_SFTP_UNIQUE_PTR(sftp_stat, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_lstat, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_opendir, sftp_closedir)
MP_SFTP_UNIQUE_PTR(sftp_readdir, sftp_attributes_free)
MP_SFTP_UNIQUE_PTR(sftp_readlink, free)

using SFTPAttributesUPtr = std::unique_ptr<sftp_attributes_struct, std::function<void(sftp_attributes)>>;
using SFTPDirUPtr = std::unique_ptr<sftp_dir_struct, std::function<int(sftp_dir)>>;

class SFTPDirIterator
{
    sftp_session sftp;
    std::stack<SFTPDirUPtr, std::vector<SFTPDirUPtr>> dirs;
    SFTPAttributesUPtr previous_attr;

    void push_dir(const std::string& path);

public:
    SFTPDirIterator(sftp_session sftp, const std::string& path);
    bool hasNext();
    SFTPAttributesUPtr next();
};

} // namespace multipass
#endif // MULTIPASS_SFTP_UTILS_H

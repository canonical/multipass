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

#ifndef MULTIPASS_SFTP_DIR_ITERATOR_H
#define MULTIPASS_SFTP_DIR_ITERATOR_H

#include <filesystem>
#include <functional>
#include <libssh/sftp.h>
#include <memory>
#include <stack>
#include <vector>

namespace fs = std::filesystem;

namespace multipass
{
using SFTPAttributesUPtr = std::unique_ptr<sftp_attributes_struct, std::function<void(sftp_attributes)>>;
using SFTPDirUPtr = std::unique_ptr<sftp_dir_struct, std::function<int(sftp_dir)>>;

class SFTPDirIterator
{
    sftp_session sftp;
    std::stack<SFTPDirUPtr, std::vector<SFTPDirUPtr>> dirs;
    SFTPAttributesUPtr previous_attr;

    void push_dir(const std::string& path);

public:
    SFTPDirIterator() = default;
    SFTPDirIterator(sftp_session sftp, const fs::path& path);
    virtual bool hasNext();
    virtual SFTPAttributesUPtr next();

    virtual ~SFTPDirIterator() = default;
};
} // namespace multipass

#endif

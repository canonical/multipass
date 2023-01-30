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

#ifndef MULTIPASS_SFTP_DIR_ITERATOR_H
#define MULTIPASS_SFTP_DIR_ITERATOR_H

#include <libssh/sftp.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <stack>
#include <vector>

namespace multipass
{
namespace fs = std::filesystem;

using SFTPAttributesUPtr = std::unique_ptr<sftp_attributes_struct, std::function<void(sftp_attributes)>>;
using SFTPDirUPtr = std::unique_ptr<sftp_dir_struct, std::function<int(sftp_dir)>>;

class SFTPDirIterator
{
public:
    SFTPDirIterator() = default;
    SFTPDirIterator(sftp_session sftp, const fs::path& path);
    virtual bool hasNext() const;
    virtual SFTPAttributesUPtr next();

    virtual ~SFTPDirIterator() = default;

private:
    sftp_session sftp;
    std::stack<SFTPDirUPtr, std::vector<SFTPDirUPtr>> dirs;
    SFTPAttributesUPtr next_attr;

    void push_dir(const std::string& path);
};
} // namespace multipass

#endif

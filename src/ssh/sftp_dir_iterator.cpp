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

#include <multipass/format.h>
#include <multipass/ssh/sftp_dir_iterator.h>
#include <multipass/ssh/sftp_utils.h>

#include <stdexcept>

namespace multipass
{
void SFTPDirIterator::push_dir(const std::string& path)
{
    auto dir = mp_sftp_opendir(sftp, path.c_str());
    if (!dir)
        throw SFTPError{"cannot open remote directory '{}': {}", path, ssh_get_error(sftp->session)};

    dirs.push(std::move(dir));
}

SFTPDirIterator::SFTPDirIterator(sftp_session sftp, const fs::path& path) : sftp(sftp)
{
    push_dir(path.u8string());
    next();
}

bool SFTPDirIterator::hasNext() const
{
    return bool{next_attr};
}

SFTPAttributesUPtr SFTPDirIterator::next()
{
    if (dirs.empty())
        return std::move(next_attr);

    auto dir = dirs.top().get();
    while (auto attr = mp_sftp_readdir(sftp, dir))
    {
        if (strcmp(attr->name, ".") == 0 || strcmp(attr->name, "..") == 0)
            continue;

        auto path = fmt::format("{}/{}", dir->name, attr->name);
        if (attr->type == SSH_FILEXFER_TYPE_DIRECTORY)
            push_dir(path);

        attr->name = strdup(path.c_str());
        next_attr.swap(attr);
        return attr;
    }

    if (sftp_dir_eof(dir))
    {
        dirs.pop();
        return next();
    }

    SFTPError err{"cannot read remote directory '{}': {}", dir->name, ssh_get_error(sftp->session)};
    dirs.pop();
    throw err;
}
} // namespace multipass

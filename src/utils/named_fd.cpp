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

#include <multipass/utils/named_fd.h>

namespace fs = std::filesystem;
namespace mp = multipass;

namespace
{
void close_handle(int handle)
{
    if (handle != -1)
        ::close(handle);
}
}; // namespace

mp::NamedFd::NamedFd(const fs::path& path, int fd) : path{path}, fd{fd}
{
}

mp::NamedFd::NamedFd(NamedFd&& other) noexcept : path{std::move(other.path)}, fd{other.fd}
{
    other.fd = -1;
}

mp::NamedFd& mp::NamedFd::operator=(NamedFd&& other) noexcept
{
    if (this != &other)
    {
        close_handle(fd);
        path = std::move(other.path);
        fd = other.fd;
        other.fd = -1;
    }
    return *this;
}
mp::NamedFd::~NamedFd()
{
    close_handle(fd);
}

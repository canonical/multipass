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

#include <filesystem>
#include <multipass/posix.h>

namespace multipass
{
namespace fs = std::filesystem;

struct NamedFd
{
    NamedFd(const fs::path& path, int fd);
    ~NamedFd();

    NamedFd(NamedFd&&) noexcept;
    NamedFd& operator=(NamedFd&&) noexcept;
    // No copy to avoid handle leaks
    NamedFd(const NamedFd&) = delete;
    NamedFd& operator=(const NamedFd&) = delete;

    fs::path path;
    int fd;
};
} // namespace multipass

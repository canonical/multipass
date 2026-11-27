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

#include <libssh/sftp.h>

#include <filesystem>
#include <iostream>

#include <QFlags>

namespace multipass
{
namespace fs = std::filesystem;

class SFTPClient
{
public:
    enum class Flag
    {
        Recursive = 1,
        MakeParent = 2,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    virtual ~SFTPClient() = default;

    virtual bool is_remote_dir(const fs::path& path) = 0;
    virtual bool push(const fs::path& source_path,
                      const fs::path& target_path,
                      Flags flags = {}) = 0;
    virtual bool pull(const fs::path& source_path,
                      const fs::path& target_path,
                      Flags flags = {}) = 0;
    virtual void from_cin(std::istream& cin, const fs::path& target_path, bool make_parent) = 0;
    virtual void to_cout(const fs::path& source_path, std::ostream& cout) = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(SFTPClient::Flags)
} // namespace multipass

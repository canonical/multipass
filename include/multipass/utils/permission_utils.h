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

#include <multipass/singleton.h>

#include <filesystem>

#define MP_PERMISSIONS multipass::PermissionUtils::instance()

namespace multipass
{
namespace fs = std::filesystem;

enum Permissions
{
    all_all = 0777,
    read_all = 0444,
    write_all = 0222,
    exec_all = 0111,
    read_user = 0400,
    write_user = 0200,
    exec_user = 0100,
    read_group = 040,
    write_group = 020,
    exec_group = 010,
    read_other = 04,
    write_other = 02,
    exec_other = 01
};

class PermissionUtils : public Singleton<PermissionUtils>
{
public:
    PermissionUtils(const PrivatePass&) noexcept;

    // sets owner to root and sets permissions recursively such that only owner has access.
    virtual void restrict_permissions(const fs::path& path) const;
};
} // namespace multipass

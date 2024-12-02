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

#ifndef MULTIPASS_PERMISSION_UTILS_H
#define MULTIPASS_PERMISSION_UTILS_H

#include <multipass/file_ops.h>
#include <multipass/platform.h>
#include <multipass/singleton.h>

#define MP_PERMISSIONS multipass::PermissionUtils::instance()

namespace multipass
{

class PermissionUtils : public Singleton<PermissionUtils>
{
public:
    PermissionUtils(const PrivatePass&) noexcept;

    virtual void set_permissions(const fs::path& path, const QFileDevice::Permissions& permissions) const;
    virtual void take_ownership(const fs::path& path, bool root = true) const;

    // sets owner to root and sets permissions such that only owner has access.
    virtual void restrict_permissions(const fs::path& path) const;
};
} // namespace multipass

#endif // MULTIPASS_PERMISSION_UTILS_H

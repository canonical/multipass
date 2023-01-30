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

#include <multipass/standard_paths.h>

namespace mp = multipass;

mp::StandardPaths::StandardPaths(const Singleton<StandardPaths>::PrivatePass& pass) noexcept
    : Singleton<StandardPaths>::Singleton{pass}
{
}

QString mp::StandardPaths::locate(StandardLocation type, const QString& fileName, LocateOptions options) const
{
    return QStandardPaths::locate(type, fileName, options);
}

QStringList mp::StandardPaths::standardLocations(StandardLocation type) const
{
    return QStandardPaths::standardLocations(type);
}

QString mp::StandardPaths::writableLocation(StandardLocation type) const
{
    return QStandardPaths::writableLocation(type);
}

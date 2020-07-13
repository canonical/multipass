/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/snap_utils.h>

#include <QFileInfo>

namespace mp = multipass;
namespace mu = multipass::utils;

QByteArray mu::snap_dir()
{
    auto snap_dir = qgetenv("SNAP");                         // Inside snap, this can be trusted.
    if (snap_dir.isEmpty())
    {
        throw mp::SnapEnvironmentException("SNAP");
    }

    return QFileInfo(snap_dir).canonicalFilePath().toUtf8(); // To resolve any symlinks
}

QByteArray mu::snap_common_dir()
{
    auto snap_common = qgetenv("SNAP_COMMON");                  // Inside snap, this can be trusted
    if (snap_common.isEmpty())
    {
        throw mp::SnapEnvironmentException("SNAP_COMMON");
    }

    return QFileInfo(snap_common).canonicalFilePath().toUtf8(); // To resolve any symlinks
}

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

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/snap_utils.h>

#include <QFileInfo>
#include <QString>

namespace mp = multipass;
namespace mpu = multipass::utils;

namespace
{
const QString snap_name{"multipass"};

void verify_snap_name()
{
    if (!mpu::in_multipass_snap())
        throw mp::SnapEnvironmentException("SNAP_NAME", snap_name.toStdString());
}

QByteArray checked_snap_env_var(const char* var)
{
    verify_snap_name();

    auto ret = qgetenv(var); // Inside snap, this can be trusted.
    if (ret.isEmpty())
        throw mp::SnapEnvironmentException(var);

    return ret;
}

QByteArray checked_snap_dir(const char* dir)
{
    return QFileInfo(checked_snap_env_var(dir)).canonicalFilePath().toUtf8(); // To resolve any symlinks
}
} // namespace

bool mpu::in_multipass_snap()
{
    return qgetenv("SNAP_NAME") == snap_name;
}

QByteArray mpu::snap_dir()
{
    return checked_snap_dir("SNAP");
}

QByteArray mpu::snap_common_dir()
{
    return checked_snap_dir("SNAP_COMMON");
}

QByteArray mpu::snap_real_home_dir()
{
    return checked_snap_dir("SNAP_REAL_HOME");
}

QByteArray mpu::snap_user_common_dir()
{
    return checked_snap_dir("SNAP_USER_COMMON");
}

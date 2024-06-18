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

#include "qemu_base_process_spec.h"

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/snap_utils.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

QString mp::QemuBaseProcessSpec::program() const
{
    return QString("qemu-system-%1").arg(HOST_ARCH);
}

QString mp::QemuBaseProcessSpec::working_directory() const
{
    try
    {
        return mpu::snap_dir().append("/qemu");
    }
    catch (const mp::SnapEnvironmentException&)
    {
        return QString();
    }
}

QString mp::QemuBaseProcessSpec::apparmor_profile() const
{
    return QString();
}

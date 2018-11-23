/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "qemuimg_process_spec.h"


multipass::QemuImgProcessSpec::QemuImgProcessSpec(const QString& input_image_path, const QString& output_image_path)
    : input_image_path{input_image_path}, output_image_path{output_image_path}

{
}

QString multipass::QemuImgProcessSpec::program() const
{
    return QStringLiteral("qemu-img");
}

QString multipass::QemuImgProcessSpec::apparmor_profile() const
{
    QString profile_template(R"END(
#include <tunables/global>
profile %1 flags=(attach_disconnected) {
    #include <abstractions/base>

    %2

    # Disk image(s) to operate on
    %3 rk,
    %4
}
    )END");

    // If running as a snap, presuming fully confined, so need to add rule to allow mmap of binary to be launched.
    QString executable;
    const QString snap_dir = qgetenv("SNAP");
    if (!snap_dir.isEmpty())
    {
        executable = QString("%1/usr/bin/qemu-img mr,").arg(snap_dir);
    }

    QString optional_output_rule;
    if (!output_image_path.isNull())
    {
        optional_output_rule = output_image_path + " rwk,";
    }

    return profile_template.arg(apparmor_profile_name(), executable, input_image_path, optional_output_rule);
}

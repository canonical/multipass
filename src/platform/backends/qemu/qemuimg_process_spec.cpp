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


multipass::QemuImgProcessSpec::QemuImgProcessSpec(const QString &image_path)
    : image_path{image_path}
{}

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
    #include <abstractions/consoles>

    # required for reading disk images
    capability dac_override,
    #capability dac_read_search,
    #capability chown,

    # needed to drop privileges
    #capability setgid,
    #capability setuid,

    # Disk image to operate on
    %3 rk,  # QCow2 filesystem image
}
    )END");

    return profile_template.arg(apparmor_profile_name(), image_path);}

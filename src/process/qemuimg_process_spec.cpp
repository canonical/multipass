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
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/snap_utils.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

mp::QemuImgProcessSpec::QemuImgProcessSpec(const QStringList& args, const QString& source_image,
                                           const QString& target_image)
    : args{args}, source_image{source_image}, target_image{target_image}
{
}

QString mp::QemuImgProcessSpec::program() const
{
    return "qemu-img";
}

QStringList mp::QemuImgProcessSpec::arguments() const
{
    return args;
}

QString mp::QemuImgProcessSpec::apparmor_profile() const
{
    QString profile_template(R"END(
#include <tunables/global>
profile %1 flags=(attach_disconnected) {
  #include <abstractions/base>

  capability ipc_lock,
  capability dac_read_search,
  %2

  # binary and its libs
  %3/usr/bin/%4 ixr,
  %3/{usr/,}lib/@{multiarch}/{,**/}*.so* rm,

  # CLASSIC ONLY: need to specify required libs from core snap
  /{,var/lib/snapd/}snap/core18/*/{,usr/}lib/@{multiarch}/{,**/}*.so* rm,

  # Images
%5

  # Allow multipassd send qemu-img signals
  signal (receive) peer=%6,
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir; // root directory: either "" or $SNAP
    QString images;
    QString extra_capabilities;
    QString signal_peer; // who can send kill signal to qemu-img

    try
    {
        root_dir = mpu::snap_dir();
        signal_peer = "snap.multipass.multipassd"; // only multipassd can send qemu-img signals
    }
    catch (mp::SnapEnvironmentException&)
    {
        extra_capabilities = "capability dac_override,"; // FIXME - unclear why this is required when
                                                         // not snap confined
        signal_peer = "unconfined";
    }

    if (!source_image.isEmpty())
        images.append(QString("  %1 rwk,\n").arg(source_image)); // allow amending to qcow2 v3

    if (!target_image.isEmpty())
        images.append(QString("  %1 rwk,\n").arg(target_image));

    return profile_template.arg(apparmor_profile_name(), extra_capabilities, root_dir, program(), images, signal_peer);
}

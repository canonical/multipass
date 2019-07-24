/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <multipass/snap_utils.h>

namespace mp = multipass;
namespace mu = multipass::utils;

mp::QemuImgProcessSpec::QemuImgProcessSpec(const QStringList& args) : args{args}
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

  %2

  # binary and its libs
  %3/usr/bin/%4 ixr,
  %3/{usr/,}lib/@{multiarch}/{,**/}*.so* rm,

  # CLASSIC ONLY: need to specify required libs from core snap
  /snap/core/*/{,usr/}lib/@{multiarch}/{,**/}*.so* rm,

  # Subdirectory containing disk image(s)
  %5/** rwk,

  # Allow multipassd send qemu-img signals
  signal (receive) peer=%6,
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir; // root directory: either "" or $SNAP
    QString image_dir;
    QString extra_capabilities;
    QString signal_peer; // who can send kill signal to qemu-img

    if (mu::is_snap())
    {
        root_dir = mu::snap_dir();
        image_dir = mu::snap_common_dir();         // FIXME - am guessing we work inside this directory
        signal_peer = "snap.multipass.multipassd"; // only multipassd can send qemu-img signals
    }
    else
    {
        extra_capabilities =
            "capability dac_read_search,\n    capability dac_override,"; // FIXME - unclear why this is required when
                                                                         // not snap confined
        signal_peer = "unconfined";
        image_dir = ""; // FIXME - Do not know where disk images might be, as passed by argument
    }

    return profile_template.arg(apparmor_profile_name(), extra_capabilities, root_dir, program(), image_dir,
                                signal_peer);
}

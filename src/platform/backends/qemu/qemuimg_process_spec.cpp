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
#include "snap_utils.h"

namespace mp = multipass;
namespace ms = multipass::snap;

mp::QemuImgProcessSpec::QemuImgProcessSpec(const QString& input_image_path, const QString& output_image_path)
    : input_image_path{input_image_path}, output_image_path{output_image_path}

{
}

QString mp::QemuImgProcessSpec::program() const
{
    return QStringLiteral("qemu-img");
}

QString mp::QemuImgProcessSpec::apparmor_profile() const
{
    QString profile_template(R"END(
#include <tunables/global>
 profile %1 flags=(attach_disconnected) {
    #include <abstractions/base>

    %2
    capability dac_read_search, # GERRY only unconfined?
    capability dac_override,

    # binary and its libs
    %3/usr/bin/qemu-img ixr,
    %3/{usr/,}lib/** rm,

    # Disk image(s) to operate on
    %4 rwk, # image verification requires write access
    %5
}
    )END");

    QString optional_output_rule, extra_capabilities;
    if (!output_image_path.isNull())
    {
        optional_output_rule = output_image_path + " rwk,";
    }

    if (!ms::is_snap_confined())
    {
        // FIXME - unclear why this is required when not snap confined
        extra_capabilities = "capability dac_read_search,\n    capability dac_override,";
    }

    return profile_template.arg(apparmor_profile_name(), extra_capabilities, ms::snap_dir(), input_image_path, optional_output_rule);
}

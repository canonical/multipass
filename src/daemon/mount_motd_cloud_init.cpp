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

#include "mount_motd_cloud_init.h"

namespace mp = multipass;

namespace
{
constexpr auto script_path = "/etc/update-motd.d/60-multipass-mounts";

// mount_motd.sh, baked in at configure time
constexpr auto script =
#include "mount_motd.sh.inc"
    ;
} // namespace

YAML::Node mp::mount_motd_cloud_init()
{
    YAML::Node entry;
    entry["path"] = script_path;
    entry["permissions"] = "0755";
    entry["content"] = script;

    return entry;
}

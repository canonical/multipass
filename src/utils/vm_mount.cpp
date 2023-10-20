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

#include <multipass/vm_mount.h>

namespace mp = multipass;

mp::VMMount::VMMount(const std::string& sourcePath,
                     id_mappings gidMappings,
                     id_mappings uidMappings,
                     MountType mountType)
    : source_path(sourcePath),
      gid_mappings(std::move(gidMappings)),
      uid_mappings(std::move(uidMappings)),
      mount_type(mountType)
{
}

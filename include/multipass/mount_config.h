/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#ifndef MULTIPASS_MOUNT_CONFIG_H
#define MULTIPASS_MOUNT_CONFIG_H

#include <multipass/id_mappings.h>
#include <multipass/virtual_machine.h>

#include <unordered_map>

namespace multipass
{

struct MountConfig
{
    VirtualMachine* vm;
    id_mappings uid_mappings;
};

} // namespace multipass
#endif // MULTIPASS_MOUNT_CONFIG_H

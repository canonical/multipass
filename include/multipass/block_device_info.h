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

#ifndef MULTIPASS_BLOCK_DEVICE_INFO_H
#define MULTIPASS_BLOCK_DEVICE_INFO_H

#include <multipass/memory_size.h>
#include <multipass/path.h>
#include <optional>
#include <string>

namespace multipass
{
struct BlockDeviceInfo
{
    std::string name;
    Path image_path;
    MemorySize size;
    std::optional<std::string> attached_vm;
    std::string format{"qcow2"};
};
} // namespace multipass
#endif // MULTIPASS_BLOCK_DEVICE_INFO_H

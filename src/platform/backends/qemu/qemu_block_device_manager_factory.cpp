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

#include "qemu_block_device_manager_factory.h"
#include "qemu_block_device_manager.h"

namespace mp = multipass;

using namespace multipass;

mp::BlockDeviceManager::UPtr mp::QemuBlockDeviceManagerFactory::create_block_device_manager(const Path& data_dir)
{
    return std::make_unique<QemuBlockDeviceManager>(data_dir);
}
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

#ifndef MULTIPASS_QEMU_BLOCK_DEVICE_MANAGER_FACTORY_H
#define MULTIPASS_QEMU_BLOCK_DEVICE_MANAGER_FACTORY_H

#include <multipass/block_device_manager_factory.h>

namespace multipass
{

class QemuBlockDeviceManagerFactory final : public BlockDeviceManagerFactory
{
public:
    BlockDeviceManager::UPtr create_block_device_manager(const Path& data_dir) override;
};

} // namespace multipass

#endif // MULTIPASS_QEMU_BLOCK_DEVICE_MANAGER_FACTORY_H
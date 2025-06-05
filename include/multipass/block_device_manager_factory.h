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

#ifndef MULTIPASS_BLOCK_DEVICE_MANAGER_FACTORY_H
#define MULTIPASS_BLOCK_DEVICE_MANAGER_FACTORY_H

#include "block_device_manager.h"
#include "disabled_copy_move.h"
#include "path.h"

#include <memory>

namespace multipass
{
class BlockDeviceManagerFactory : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<BlockDeviceManagerFactory>;
    virtual ~BlockDeviceManagerFactory() = default;
    
    virtual BlockDeviceManager::UPtr create_block_device_manager(const Path& data_dir) = 0;

protected:
    BlockDeviceManagerFactory() = default;
};
} // namespace multipass

#endif // MULTIPASS_BLOCK_DEVICE_MANAGER_FACTORY_H
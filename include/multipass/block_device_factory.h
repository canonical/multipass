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

#ifndef MULTIPASS_BLOCK_DEVICE_FACTORY_H
#define MULTIPASS_BLOCK_DEVICE_FACTORY_H

#include "block_device.h"
#include "disabled_copy_move.h"
#include "memory_size.h"
#include "path.h"

#include <memory>
#include <string>

namespace multipass
{
class BlockDeviceFactory : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<BlockDeviceFactory>;
    virtual ~BlockDeviceFactory() = default;
    
    virtual BlockDevice::UPtr create_block_device(const std::string& name,
                                                   const MemorySize& size,
                                                   const Path& data_dir) = 0;
    
    virtual BlockDevice::UPtr create_block_device_from_file(const std::string& name,
                                                             const std::string& source_path,
                                                             const Path& data_dir) = 0;
    
    virtual BlockDevice::UPtr load_block_device(const std::string& name,
                                                 const Path& image_path,
                                                 const MemorySize& size,
                                                 const std::string& format = "qcow2",
                                                 const std::optional<std::string>& attached_vm = std::nullopt) = 0;

protected:
    BlockDeviceFactory() = default;
};
} // namespace multipass

#endif // MULTIPASS_BLOCK_DEVICE_FACTORY_H
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

#ifndef MULTIPASS_BLOCK_DEVICE_MANAGER_H
#define MULTIPASS_BLOCK_DEVICE_MANAGER_H

#include "block_device_info.h"
#include "disabled_copy_move.h"
#include "memory_size.h"

#include <memory>
#include <string>
#include <vector>

namespace multipass
{
class BlockDeviceManager : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<BlockDeviceManager>;
    virtual ~BlockDeviceManager() = default;

    virtual void create_block_device(const std::string& name, const MemorySize& size) = 0;
    virtual void delete_block_device(const std::string& name) = 0;
    virtual void attach_block_device(const std::string& name, const std::string& vm) = 0;
    virtual void detach_block_device(const std::string& name, const std::string& vm) = 0;
    
    virtual bool has_block_device(const std::string& name) const = 0;
    virtual const BlockDeviceInfo* get_block_device(const std::string& name) const = 0;
    virtual std::vector<BlockDeviceInfo> list_block_devices() const = 0;
    virtual void register_block_device(const BlockDeviceInfo& info) = 0;
    virtual void unregister_block_device(const std::string& name) = 0;

protected:
    BlockDeviceManager() = default;
};
} // namespace multipass

#endif // MULTIPASS_BLOCK_DEVICE_MANAGER_H
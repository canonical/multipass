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

#ifndef MULTIPASS_BLOCK_DEVICE_H
#define MULTIPASS_BLOCK_DEVICE_H

#include "disabled_copy_move.h"
#include "memory_size.h"
#include "path.h"

#include <memory>
#include <optional>
#include <string>

namespace multipass
{
class BlockDevice : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<BlockDevice>;
    
    virtual ~BlockDevice() = default;
    
    // Core properties
    virtual const std::string& name() const = 0;
    virtual const Path& image_path() const = 0;
    virtual const MemorySize& size() const = 0;
    virtual const std::string& format() const = 0;
    virtual const std::optional<std::string>& attached_vm() const = 0;
    
    // Operations
    virtual void attach_to_vm(const std::string& vm_name) = 0;
    virtual void detach_from_vm() = 0;
    virtual void delete_device() = 0;
    
    // State queries
    virtual bool is_attached() const = 0;
    virtual bool exists() const = 0;

protected:
    BlockDevice() = default;
};
} // namespace multipass

#endif // MULTIPASS_BLOCK_DEVICE_H
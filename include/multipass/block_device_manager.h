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

#include "block_device.h"
#include "block_device_factory.h"
#include "disabled_copy_move.h"
#include "memory_size.h"
#include "path.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace multipass
{
class BlockDeviceManager : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<BlockDeviceManager>;

    explicit BlockDeviceManager(BlockDeviceFactory::UPtr factory, const Path& data_dir);
    virtual ~BlockDeviceManager() = default;

    // Block device operations that manage the registry
    BlockDevice::UPtr create_block_device(const std::string& name, const MemorySize& size);
    BlockDevice::UPtr create_block_device_from_file(const std::string& name,
                                                    const std::string& source_path);
    void delete_block_device(const std::string& name);
    void attach_block_device(const std::string& name, const std::string& vm);
    void detach_block_device(const std::string& name, const std::string& vm);

    // Registry management
    bool has_block_device(const std::string& name) const;
    BlockDevice* get_block_device(const std::string& name) const;
    std::vector<const BlockDevice*> list_block_devices() const;
    void register_block_device(BlockDevice::UPtr device);
    void unregister_block_device(const std::string& name);

    // Registry synchronization
    void sync_registry_with_filesystem();

protected:
    void save_metadata() const;
    void load_metadata();

private:
    BlockDeviceFactory::UPtr device_factory;
    std::unordered_map<std::string, BlockDevice::UPtr> block_devices;
    const Path data_dir;
    const Path metadata_path;
};
} // namespace multipass

#endif // MULTIPASS_BLOCK_DEVICE_MANAGER_H

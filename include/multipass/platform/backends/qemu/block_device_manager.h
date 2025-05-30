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

#include <multipass/memory_size.h>
#include <multipass/path.h>
#include <multipass/singleton.h>
#include <multipass/block_device_info.h>

#include <QJsonObject>
#include <QString>

#include <optional>
#include <string>
#include <unordered_map>

namespace multipass
{
class BlockDeviceManager : public Singleton<BlockDeviceManager>
{
public:
    void create_block_device(const std::string& name, const MemorySize& size);
    void delete_block_device(const std::string& name);
    void attach_block_device(const std::string& name, const std::string& vm);
    void detach_block_device(const std::string& name, const std::string& vm);
    
    bool has_block_device(const std::string& name) const;
    const BlockDeviceInfo* get_block_device(const std::string& name) const;
    std::vector<BlockDeviceInfo> list_block_devices() const;

private:
    friend Singleton<BlockDeviceManager>;
    BlockDeviceManager();
    
    Path get_block_device_path(const std::string& name) const;
    void save_metadata() const;
    void load_metadata();
    void validate_name(const std::string& name) const;
    void validate_not_attached(const std::string& name) const;
    void validate_not_in_use(const std::string& name) const;
    
    std::unordered_map<std::string, BlockDeviceInfo> block_devices;
    const Path data_dir;
    const Path images_dir;
    const Path metadata_path;
};
} // namespace multipass
#endif // MULTIPASS_BLOCK_DEVICE_MANAGER_H
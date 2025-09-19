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

#pragma once

#include <multipass/disabled_copy_move.h>
#include <multipass/path.h>

#include <memory>
#include <string>
#include <vector>

namespace multipass
{

struct BlockDeviceInfo
{
    std::string id;
    std::string name;
    std::string path;
    std::string size;
    std::string backend;
    std::string attached_instance;
    std::string status;
};

class BlockDeviceManager : private DisabledCopyMove
{
public:
    virtual ~BlockDeviceManager() = default;

    virtual std::string create_block_device(const std::string& name,
                                            const std::string& size,
                                            const std::string& format = "qcow2") = 0;
    virtual void delete_block_device(const std::string& id) = 0;
    virtual void attach_block_device(const std::string& id, const std::string& instance_name) = 0;
    virtual void detach_block_device(const std::string& id) = 0;
    virtual std::vector<BlockDeviceInfo> list_block_devices() = 0;
    virtual BlockDeviceInfo get_block_device(const std::string& id) = 0;
    virtual std::vector<std::string> get_attached_devices_for_instance(
        const std::string& instance_name) = 0;

protected:
    BlockDeviceManager() = default;
};

std::unique_ptr<BlockDeviceManager> create_rust_block_device_manager(
    const std::string& storage_path);

} // namespace multipass

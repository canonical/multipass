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

#include <multipass/block_device_manager.h>
#include <multipass/logging/log.h>

#include <rustipass_cxx/lib.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
class RustBlockDeviceManager : public mp::BlockDeviceManager
{
public:
    explicit RustBlockDeviceManager(const std::string& storage_path)
        : rust_manager(multipass::block_storage::create_rust_block_device_manager(storage_path))
    {
        mpl::log(mpl::Level::debug,
                 "rust_block_device_manager",
                 fmt::format("Created RustBlockDeviceManager with storage path: {}", storage_path));
    }

    std::string create_block_device(const std::string& name,
                                    const std::string& size,
                                    const std::string& format = "qcow2") override
    {
        mpl::log(
            mpl::Level::info,
            "rust_block_device_manager",
            fmt::format("Creating block device: name={}, size={}, format={}", name, size, format));

        try
        {
            auto result =
                multipass::block_storage::create_block_device(*rust_manager, name, size, format);
            std::string result_str(result);
            mpl::log(mpl::Level::info,
                     "rust_block_device_manager",
                     fmt::format("Successfully created block device with ID: {}", result_str));
            return result_str;
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "rust_block_device_manager",
                     fmt::format("Failed to create block device: {}", e.what()));
            throw;
        }
    }

    void delete_block_device(const std::string& id) override
    {
        mpl::log(mpl::Level::info,
                 "rust_block_device_manager",
                 fmt::format("Deleting block device with ID: {}", id));

        try
        {
            multipass::block_storage::delete_block_device(*rust_manager, id);
            mpl::log(mpl::Level::info,
                     "rust_block_device_manager",
                     fmt::format("Successfully deleted block device: {}", id));
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "rust_block_device_manager",
                     fmt::format("Failed to delete block device {}: {}", id, e.what()));
            throw;
        }
    }

    void attach_block_device(const std::string& id, const std::string& instance_name) override
    {
        mpl::log(mpl::Level::info,
                 "rust_block_device_manager",
                 fmt::format("Attaching block device {} to instance {}", id, instance_name));

        try
        {
            multipass::block_storage::attach_block_device(*rust_manager, id, instance_name);
            mpl::log(mpl::Level::info,
                     "rust_block_device_manager",
                     fmt::format("Successfully attached block device {} to instance {}",
                                 id,
                                 instance_name));
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "rust_block_device_manager",
                     fmt::format("Failed to attach block device {} to instance {}: {}",
                                 id,
                                 instance_name,
                                 e.what()));
            throw;
        }
    }

    void detach_block_device(const std::string& id) override
    {
        mpl::log(mpl::Level::info,
                 "rust_block_device_manager",
                 fmt::format("Detaching block device with ID: {}", id));

        try
        {
            multipass::block_storage::detach_block_device(*rust_manager, id);
            mpl::log(mpl::Level::info,
                     "rust_block_device_manager",
                     fmt::format("Successfully detached block device: {}", id));
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "rust_block_device_manager",
                     fmt::format("Failed to detach block device {}: {}", id, e.what()));
            throw;
        }
    }

    std::vector<mp::BlockDeviceInfo> list_block_devices() override
    {
        mpl::log(mpl::Level::debug, "rust_block_device_manager", "Listing all block devices");

        try
        {
            auto rust_devices = multipass::block_storage::list_block_devices_ffi(*rust_manager);
            std::vector<mp::BlockDeviceInfo> devices;

            for (const auto& rust_device : rust_devices)
            {
                mp::BlockDeviceInfo device;
                device.id = ""; // Not provided by Rust FFI yet
                device.name = std::string(rust_device.name);
                device.path = std::string(rust_device.path);
                device.size = std::string(rust_device.size);
                device.backend = "qemu"; // Default backend
                device.attached_instance = std::string(rust_device.attached_to);
                device.status = "available"; // Default status
                devices.push_back(device);
            }

            mpl::log(mpl::Level::debug,
                     "rust_block_device_manager",
                     fmt::format("Found {} block devices", devices.size()));
            return devices;
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "rust_block_device_manager",
                     fmt::format("Failed to list block devices: {}", e.what()));
            throw;
        }
    }

    mp::BlockDeviceInfo get_block_device(const std::string& id) override
    {
        mpl::log(mpl::Level::debug,
                 "rust_block_device_manager",
                 fmt::format("Getting block device with ID: {}", id));

        try
        {
            auto rust_device = multipass::block_storage::get_block_device_ffi(*rust_manager, id);

            mp::BlockDeviceInfo device;
            device.id = ""; // Not provided by Rust FFI yet
            device.name = std::string(rust_device.name);
            device.path = std::string(rust_device.path);
            device.size = std::string(rust_device.size);
            device.backend = "qemu"; // Default backend
            device.attached_instance = std::string(rust_device.attached_to);
            device.status = "available"; // Default status

            mpl::log(mpl::Level::debug,
                     "rust_block_device_manager",
                     fmt::format("Retrieved block device: {}", device.name));
            return device;
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "rust_block_device_manager",
                     fmt::format("Failed to get block device {}: {}", id, e.what()));
            throw;
        }
    }

    std::vector<std::string> get_attached_devices_for_instance(
        const std::string& instance_name) override
    {
        mpl::log(mpl::Level::debug,
                 "rust_block_device_manager",
                 fmt::format("Getting attached devices for instance: {}", instance_name));

        try
        {
            auto rust_devices =
                multipass::block_storage::get_attached_devices_for_instance(*rust_manager,
                                                                            instance_name);
            std::vector<std::string> devices;
            for (const auto& rust_device : rust_devices)
            {
                devices.push_back(std::string(rust_device));
            }
            mpl::log(mpl::Level::debug,
                     "rust_block_device_manager",
                     fmt::format("Found {} attached devices for instance {}",
                                 devices.size(),
                                 instance_name));
            return devices;
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "rust_block_device_manager",
                     fmt::format("Failed to get attached devices for instance {}: {}",
                                 instance_name,
                                 e.what()));
            throw;
        }
    }

private:
    rust::cxxbridge1::Box<multipass::block_storage::BlockDeviceManager> rust_manager;
};

} // namespace

std::unique_ptr<mp::BlockDeviceManager> mp::create_rust_block_device_manager(
    const std::string& storage_path)
{
    mpl::log(mpl::Level::info,
             "rust_block_device_manager",
             fmt::format("Creating RustBlockDeviceManager with storage path: {}", storage_path));
    return std::make_unique<RustBlockDeviceManager>(storage_path);
}

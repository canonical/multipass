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

#include <multipass/constants.h>
#include <multipass/exceptions/block_device_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace multipass;

namespace
{
constexpr auto block_devices_dir = "block-devices";
constexpr auto metadata_file = "metadata.json";

QJsonObject block_device_to_json(const mp::BlockDevice& device)
{
    QJsonObject json;
    json["name"] = QString::fromStdString(device.name());
    json["path"] = device.image_path();
    json["size"] = QString::number(device.size().in_bytes());
    if (device.attached_vm())
        json["attached_vm"] = QString::fromStdString(*device.attached_vm());
    json["format"] = QString::fromStdString(device.format());
    return json;
}
} // namespace

mp::BlockDeviceManager::BlockDeviceManager(BlockDeviceFactory::UPtr factory, const Path& data_dir)
    : device_factory{std::move(factory)},
      data_dir{MP_UTILS.make_dir(QDir(data_dir), block_devices_dir)},
      metadata_path{this->data_dir + "/" + metadata_file}
{
    load_metadata();
}

mp::BlockDevice::UPtr mp::BlockDeviceManager::create_block_device(const std::string& name,
                                                                  const MemorySize& size)
{
    if (has_block_device(name))
        throw ValidationError(fmt::format("Block device '{}' already exists", name));

    auto device = device_factory->create_block_device(name, size, data_dir);
    auto* device_ptr = device.get();

    // Store in registry
    block_devices[name] = std::move(device);
    save_metadata();

    mpl::log(mpl::Level::info, "block-device", fmt::format("Created block device '{}'", name));

    // Return a copy for the caller (they get ownership)
    return device_factory->load_block_device(device_ptr->name(),
                                             device_ptr->image_path(),
                                             device_ptr->size(),
                                             device_ptr->format(),
                                             device_ptr->attached_vm());
}

mp::BlockDevice::UPtr mp::BlockDeviceManager::create_block_device_from_file(
    const std::string& name,
    const std::string& source_path)
{
    if (has_block_device(name))
        throw ValidationError(fmt::format("Block device '{}' already exists", name));

    // Validate source file exists
    QFileInfo source_file(QString::fromStdString(source_path));
    if (!source_file.exists())
        throw ValidationError(fmt::format("Source file '{}' does not exist", source_path));

    if (!source_file.isFile())
        throw ValidationError(fmt::format("Source path '{}' is not a regular file", source_path));

    auto device = device_factory->create_block_device_from_file(name, source_path, data_dir);
    auto* device_ptr = device.get();

    // Store in registry
    block_devices[name] = std::move(device);
    save_metadata();

    mpl::log(mpl::Level::info,
             "block-device",
             fmt::format("Created block device '{}' from file '{}'", name, source_path));

    // Return a copy for the caller (they get ownership)
    return device_factory->load_block_device(device_ptr->name(),
                                             device_ptr->image_path(),
                                             device_ptr->size(),
                                             device_ptr->format(),
                                             device_ptr->attached_vm());
}

void mp::BlockDeviceManager::delete_block_device(const std::string& name)
{
    auto* device = get_block_device(name);
    if (!device)
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));

    device->delete_device();
    block_devices.erase(name);
    save_metadata();

    mpl::log(mpl::Level::info, "block-device", fmt::format("Deleted block device '{}'", name));
}

void mp::BlockDeviceManager::attach_block_device(const std::string& name, const std::string& vm)
{
    auto* device = get_block_device(name);
    if (!device)
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));

    device->attach_to_vm(vm);
    save_metadata();

    mpl::log(mpl::Level::info,
             "block-device",
             fmt::format("Attached block device '{}' to VM '{}'", name, vm));
}

void mp::BlockDeviceManager::detach_block_device(const std::string& name, const std::string& vm)
{
    auto* device = get_block_device(name);
    if (!device)
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));

    device->detach_from_vm();
    save_metadata();

    mpl::log(mpl::Level::info,
             "block-device",
             fmt::format("Detached block device '{}' from VM '{}'", name, vm));
}

bool mp::BlockDeviceManager::has_block_device(const std::string& name) const
{
    return block_devices.find(name) != block_devices.end();
}

mp::BlockDevice* mp::BlockDeviceManager::get_block_device(const std::string& name) const
{
    auto it = block_devices.find(name);
    return it != block_devices.end() ? it->second.get() : nullptr;
}

std::vector<const mp::BlockDevice*> mp::BlockDeviceManager::list_block_devices() const
{
    std::vector<const BlockDevice*> devices;
    devices.reserve(block_devices.size());
    for (const auto& [_, device] : block_devices)
        devices.push_back(device.get());
    return devices;
}

void mp::BlockDeviceManager::register_block_device(BlockDevice::UPtr device)
{
    const auto& name = device->name();

    mpl::log(
        mpl::Level::debug,
        "block-device",
        fmt::format(
            "register_block_device called with name='{}', size={}, path='{}', attached_vm='{}'",
            device->name(),
            device->size().human_readable(),
            device->image_path().toStdString(),
            device->attached_vm() ? *device->attached_vm() : "none"));

    if (block_devices.find(name) != block_devices.end())
    {
        mpl::log(mpl::Level::error,
                 "block-device",
                 fmt::format("Block device '{}' already exists, cannot register", name));
        throw ValidationError(fmt::format("Block device '{}' already exists", name));
    }

    // Verify the image file exists
    if (!device->exists())
    {
        mpl::log(mpl::Level::error,
                 "block-device",
                 fmt::format("Block device image file does not exist: {}",
                             device->image_path().toStdString()));
        throw std::runtime_error(fmt::format("Block device image file does not exist: {}",
                                             device->image_path().toStdString()));
    }

    // Register the block device
    block_devices[name] = std::move(device);
    save_metadata();

    mpl::log(
        mpl::Level::info,
        "block-device",
        fmt::format("Successfully registered block device '{}' with {} registered devices total",
                    name,
                    block_devices.size()));
}

void mp::BlockDeviceManager::unregister_block_device(const std::string& name)
{
    mpl::log(mpl::Level::debug,
             "block-device",
             fmt::format("unregister_block_device called for '{}'", name));

    auto it = block_devices.find(name);
    if (it == block_devices.end())
    {
        mpl::log(mpl::Level::warning,
                 "block-device",
                 fmt::format("Block device '{}' not found, cannot unregister", name));
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));
    }

    // Remove from registry (but don't delete the actual image file)
    block_devices.erase(it);
    save_metadata();

    mpl::log(mpl::Level::info,
             "block-device",
             fmt::format("Successfully unregistered block device '{}', {} devices remaining",
                         name,
                         block_devices.size()));
}

void mp::BlockDeviceManager::save_metadata() const
{
    QJsonObject root;
    QJsonObject devices;

    for (const auto& [name, device] : block_devices)
        devices[QString::fromStdString(name)] = block_device_to_json(*device);

    root["block_devices"] = devices;

    QJsonDocument doc(root);
    QFile file(metadata_path);
    if (!file.open(QIODevice::WriteOnly))
        throw std::runtime_error("Failed to save block device metadata");

    file.write(doc.toJson());
}

void mp::BlockDeviceManager::load_metadata()
{
    QFile file(metadata_path);
    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Failed to load block device metadata");

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    auto root = doc.object();
    auto devices = root["block_devices"].toObject();

    for (const auto& key : devices.keys())
    {
        auto device_obj = devices[key].toObject();
        auto name = device_obj["name"].toString().toStdString();
        auto path = device_obj["path"].toString();
        auto size = mp::MemorySize{device_obj["size"].toString().toStdString()};
        auto format = device_obj["format"].toString().toStdString();
        std::optional<std::string> attached_vm;
        if (device_obj.contains("attached_vm"))
            attached_vm = device_obj["attached_vm"].toString().toStdString();

        auto device = device_factory->load_block_device(name, path, size, format, attached_vm);
        block_devices[name] = std::move(device);
    }
}

void mp::BlockDeviceManager::sync_registry_with_filesystem()
{
    mpl::log(mpl::Level::info,
             "block-device",
             "Synchronizing block device registry with filesystem");

    std::vector<std::string> devices_to_remove;

    for (const auto& [name, device] : block_devices)
    {
        // Check if the device file actually exists
        if (!device->exists())
        {
            mpl::log(mpl::Level::warning,
                     "block-device",
                     fmt::format("Block device '{}' file does not exist: {}",
                                 name,
                                 device->image_path().toStdString()));
            devices_to_remove.push_back(name);
            continue;
        }

        // Check if the attached VM still exists (if device claims to be attached)
        if (device->attached_vm())
        {
            const auto& attached_vm = *device->attached_vm();
            // We'll need to check this against the daemon's instance list
            // For now, we'll mark devices attached to non-existent VMs for cleanup
            // This will be enhanced when we integrate with the daemon
            mpl::log(mpl::Level::debug,
                     "block-device",
                     fmt::format("Block device '{}' claims to be attached to VM '{}'",
                                 name,
                                 attached_vm));
        }
    }

    // Remove orphaned devices
    for (const auto& device_name : devices_to_remove)
    {
        mpl::log(mpl::Level::info,
                 "block-device",
                 fmt::format("Removing orphaned block device '{}'", device_name));
        try
        {
            block_devices.erase(device_name);
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error,
                     "block-device",
                     fmt::format("Failed to remove orphaned block device '{}': {}",
                                 device_name,
                                 e.what()));
        }
    }

    if (!devices_to_remove.empty())
    {
        save_metadata();
        mpl::log(mpl::Level::info,
                 "block-device",
                 fmt::format("Cleaned up {} orphaned block devices", devices_to_remove.size()));
    }
    else
    {
        mpl::log(mpl::Level::info, "block-device", "No orphaned block devices found");
    }
}

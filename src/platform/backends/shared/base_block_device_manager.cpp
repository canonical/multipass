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

#include "base_block_device_manager.h"

#include <multipass/constants.h>
#include <multipass/exceptions/block_device_exceptions.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace multipass;

namespace
{
constexpr auto block_devices_dir = "block-devices";
constexpr auto images_subdir = "images";
constexpr auto metadata_file = "metadata.json";

QJsonObject block_info_to_json(const mp::BlockDeviceInfo& info)
{
    QJsonObject json;
    json["name"] = QString::fromStdString(info.name);
    json["path"] = info.image_path;
    json["size"] = QString::number(info.size.in_bytes());
    if (info.attached_vm)
        json["attached_vm"] = QString::fromStdString(*info.attached_vm);
    json["format"] = QString::fromStdString(info.format);
    return json;
}

mp::BlockDeviceInfo json_to_block_info(const QJsonObject& json)
{
    mp::BlockDeviceInfo info;
    info.name = json["name"].toString().toStdString();
    info.image_path = json["path"].toString();
    info.size = mp::MemorySize{json["size"].toString().toStdString()};
    if (json.contains("attached_vm"))
        info.attached_vm = json["attached_vm"].toString().toStdString();
    info.format = json["format"].toString().toStdString();
    return info;
}
} // namespace

mp::BaseBlockDeviceManager::BaseBlockDeviceManager(const Path& data_dir)
    : data_dir{MP_UTILS.make_dir(QDir(data_dir), block_devices_dir)},
      images_dir{MP_UTILS.make_dir(QDir(this->data_dir), images_subdir)},
      metadata_path{this->data_dir + "/" + metadata_file}
{
    load_metadata();
}

void mp::BaseBlockDeviceManager::create_block_device(const std::string& name, const MemorySize& size)
{
    validate_name(name);
    
    if (has_block_device(name))
        throw ValidationError(fmt::format("Block device '{}' already exists", name));

    if (size < MemorySize{"1G"})
        throw ValidationError(fmt::format("Block device size must be at least 1G, got '{}'", size.in_bytes()));

    auto image_path = get_block_device_path(name);
    
    // Create the block device image using backend-specific implementation
    create_block_device_image(name, size, image_path);

    // Store metadata
    BlockDeviceInfo info{name, image_path, size, std::nullopt, "qcow2"};
    block_devices[name] = info;
    save_metadata();

    mpl::log(mpl::Level::info, "block-device", fmt::format("Created block device '{}'", name));
}

void mp::BaseBlockDeviceManager::delete_block_device(const std::string& name)
{
    validate_not_in_use(name);

    auto info = get_block_device(name);
    if (!info)
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));

    remove_block_device_image(info->image_path);
    block_devices.erase(name);
    save_metadata();

    mpl::log(mpl::Level::info, "block-device", fmt::format("Deleted block device '{}'", name));
}

void mp::BaseBlockDeviceManager::attach_block_device(const std::string& name, const std::string& vm)
{
    auto info = get_block_device(name);
    if (!info)
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));

    validate_not_attached(name);

    block_devices[name].attached_vm = vm;
    save_metadata();

    mpl::log(mpl::Level::info, "block-device",
             fmt::format("Attached block device '{}' to VM '{}'", name, vm));
}

void mp::BaseBlockDeviceManager::detach_block_device(const std::string& name, const std::string& vm)
{
    auto info = get_block_device(name);
    if (!info)
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));

    if (!info->attached_vm)
        throw ValidationError(fmt::format("Block device '{}' is not attached to any VM", name));

    if (*info->attached_vm != vm)
        throw ValidationError(
            fmt::format("Block device '{}' is attached to VM '{}', not '{}'", name, *info->attached_vm, vm));

    block_devices[name].attached_vm = std::nullopt;
    save_metadata();

    mpl::log(mpl::Level::info, "block-device",
             fmt::format("Detached block device '{}' from VM '{}'", name, vm));
}

bool mp::BaseBlockDeviceManager::has_block_device(const std::string& name) const
{
    return block_devices.find(name) != block_devices.end();
}

const mp::BlockDeviceInfo* mp::BaseBlockDeviceManager::get_block_device(const std::string& name) const
{
    auto it = block_devices.find(name);
    return it != block_devices.end() ? &it->second : nullptr;
}

std::vector<mp::BlockDeviceInfo> mp::BaseBlockDeviceManager::list_block_devices() const
{
    std::vector<BlockDeviceInfo> devices;
    devices.reserve(block_devices.size());
    for (const auto& [_, info] : block_devices)
        devices.push_back(info);
    return devices;
}

mp::Path mp::BaseBlockDeviceManager::get_block_device_path(const std::string& name) const
{
    return images_dir + "/" + QString::fromStdString(name) + ".qcow2";
}

void mp::BaseBlockDeviceManager::create_block_device_image(const std::string& name, const MemorySize& size, const Path& image_path)
{
    // Default implementation - subclasses should override this for backend-specific behavior
    throw std::runtime_error("create_block_device_image not implemented by backend");
}

void mp::BaseBlockDeviceManager::remove_block_device_image(const Path& image_path)
{
    // Default implementation - simple file removal
    QFile::remove(image_path);
}

void mp::BaseBlockDeviceManager::register_block_device(const BlockDeviceInfo& info)
{
    mpl::log(mpl::Level::debug, "block-device",
             fmt::format("register_block_device called with name='{}', size={}, path='{}', attached_vm='{}'",
                       info.name, info.size.human_readable(), info.image_path.toStdString(),
                       info.attached_vm ? *info.attached_vm : "none"));

    if (block_devices.find(info.name) != block_devices.end())
    {
        mpl::log(mpl::Level::error, "block-device",
                 fmt::format("Block device '{}' already exists, cannot register", info.name));
        throw ValidationError(fmt::format("Block device '{}' already exists", info.name));
    }

    // Verify the image file exists
    mpl::log(mpl::Level::debug, "block-device",
             fmt::format("Checking if image file exists: '{}'", info.image_path.toStdString()));
    
    if (!QFile::exists(info.image_path))
    {
        mpl::log(mpl::Level::error, "block-device",
                 fmt::format("Block device image file does not exist: {}", info.image_path.toStdString()));
        throw std::runtime_error(fmt::format("Block device image file does not exist: {}", info.image_path.toStdString()));
    }

    mpl::log(mpl::Level::debug, "block-device",
             fmt::format("Image file exists, registering block device '{}'", info.name));

    // Register the block device
    block_devices[info.name] = info;
    save_metadata();
    
    mpl::log(mpl::Level::info, "block-device",
             fmt::format("Successfully registered block device '{}' with {} registered devices total",
                       info.name, block_devices.size()));
}

void mp::BaseBlockDeviceManager::unregister_block_device(const std::string& name)
{
    mpl::log(mpl::Level::debug, "block-device",
             fmt::format("unregister_block_device called for '{}'", name));

    auto it = block_devices.find(name);
    if (it == block_devices.end())
    {
        mpl::log(mpl::Level::warning, "block-device",
                 fmt::format("Block device '{}' not found, cannot unregister", name));
        throw NotFoundError(fmt::format("Block device '{}' does not exist", name));
    }

    // Remove from registry (but don't delete the actual image file)
    block_devices.erase(it);
    save_metadata();
    
    mpl::log(mpl::Level::info, "block-device",
             fmt::format("Successfully unregistered block device '{}', {} devices remaining",
                       name, block_devices.size()));
}

void mp::BaseBlockDeviceManager::save_metadata() const
{
    QJsonObject root;
    QJsonObject devices;

    for (const auto& [name, info] : block_devices)
        devices[QString::fromStdString(name)] = block_info_to_json(info);

    root["block_devices"] = devices;

    QJsonDocument doc(root);
    QFile file(metadata_path);
    if (!file.open(QIODevice::WriteOnly))
        throw std::runtime_error("Failed to save block device metadata");

    file.write(doc.toJson());
}

void mp::BaseBlockDeviceManager::load_metadata()
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
        auto info = json_to_block_info(devices[key].toObject());
        block_devices[info.name] = info;
    }
}

void mp::BaseBlockDeviceManager::validate_name(const std::string& name) const
{
    static const QRegularExpression name_regex("^[a-zA-Z][a-zA-Z0-9-]*[a-zA-Z0-9]$");

    if (!name_regex.match(QString::fromStdString(name)).hasMatch())
        throw ValidationError(
            fmt::format("Invalid block device name '{}'. Names must start with a letter, end with a letter or digit, "
                       "and contain only letters, digits, or hyphens",
                       name));
}

void mp::BaseBlockDeviceManager::validate_not_attached(const std::string& name) const
{
    auto info = get_block_device(name);
    if (info && info->attached_vm)
        throw ValidationError(fmt::format("Block device '{}' is attached to VM '{}'", name, *info->attached_vm));
}

void mp::BaseBlockDeviceManager::validate_not_in_use(const std::string& name) const
{
    validate_not_attached(name);
}
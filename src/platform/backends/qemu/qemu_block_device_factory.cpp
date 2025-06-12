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

#include "qemu_block_device_factory.h"
#include "qemu_block_device.h"

#include <multipass/exceptions/block_device_exceptions.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/utils.h>

#include <QDir>
#include <QRegularExpression>

namespace mp = multipass;

using namespace multipass;

mp::BlockDevice::UPtr mp::QemuBlockDeviceFactory::create_block_device(const std::string& name,
                                                                       const MemorySize& size,
                                                                       const Path& data_dir)
{
    validate_name(name);
    
    if (size < MemorySize{"1G"})
        throw ValidationError(fmt::format("Block device size must be at least 1G, got '{}'", size.in_bytes()));

    auto image_path = get_block_device_path(name, data_dir);
    
    // Create the image file
    QemuBlockDevice::create_image_file(name, size, image_path);
    
    // Return the block device object
    return std::make_unique<QemuBlockDevice>(name, image_path, size);
}

mp::BlockDevice::UPtr mp::QemuBlockDeviceFactory::load_block_device(const std::string& name,
                                                                     const Path& image_path,
                                                                     const MemorySize& size,
                                                                     const std::string& format,
                                                                     const std::optional<std::string>& attached_vm)
{
    return std::make_unique<QemuBlockDevice>(name, image_path, size, format, attached_vm);
}

mp::Path mp::QemuBlockDeviceFactory::get_block_device_path(const std::string& name, const Path& data_dir) const
{
    constexpr auto block_devices_dir = "block-devices";
    constexpr auto images_subdir = "images";
    
    auto block_dir = MP_UTILS.make_dir(QDir(data_dir), block_devices_dir);
    auto images_dir = MP_UTILS.make_dir(QDir(block_dir), images_subdir);
    
    return images_dir + "/" + QString::fromStdString(name) + ".qcow2";
}

void mp::QemuBlockDeviceFactory::validate_name(const std::string& name) const
{
    static const QRegularExpression name_regex("^[a-zA-Z][a-zA-Z0-9-]*[a-zA-Z0-9]$");

    if (!name_regex.match(QString::fromStdString(name)).hasMatch())
        throw ValidationError(
            fmt::format("Invalid block device name '{}'. Names must start with a letter, end with a letter or digit, "
                       "and contain only letters, digits, or hyphens",
                       name));
}
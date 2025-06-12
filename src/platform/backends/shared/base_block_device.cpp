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

#include "base_block_device.h"

#include <multipass/exceptions/block_device_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <QFile>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::BaseBlockDevice::BaseBlockDevice(const std::string& name,
                                      const Path& image_path,
                                      const MemorySize& size,
                                      const std::string& format,
                                      const std::optional<std::string>& attached_vm)
    : device_name{name},
      device_image_path{image_path},
      device_size{size},
      device_format{format},
      device_attached_vm{attached_vm}
{
}

const std::string& mp::BaseBlockDevice::name() const
{
    return device_name;
}

const mp::Path& mp::BaseBlockDevice::image_path() const
{
    return device_image_path;
}

const mp::MemorySize& mp::BaseBlockDevice::size() const
{
    return device_size;
}

const std::string& mp::BaseBlockDevice::format() const
{
    return device_format;
}

const std::optional<std::string>& mp::BaseBlockDevice::attached_vm() const
{
    return device_attached_vm;
}

void mp::BaseBlockDevice::attach_to_vm(const std::string& vm_name)
{
    validate_attach(vm_name);
    device_attached_vm = vm_name;
    
    mpl::log(mpl::Level::info, "block-device",
             fmt::format("Attached block device '{}' to VM '{}'", device_name, vm_name));
}

void mp::BaseBlockDevice::detach_from_vm()
{
    validate_detach();
    
    auto vm_name = *device_attached_vm;
    device_attached_vm = std::nullopt;
    
    mpl::log(mpl::Level::info, "block-device",
             fmt::format("Detached block device '{}' from VM '{}'", device_name, vm_name));
}

void mp::BaseBlockDevice::delete_device()
{
    if (is_attached())
        throw ValidationError(fmt::format("Block device '{}' is attached to VM '{}', cannot delete", 
                                         device_name, *device_attached_vm));
    
    remove_image_file();
    
    mpl::log(mpl::Level::info, "block-device",
             fmt::format("Deleted block device '{}'", device_name));
}

bool mp::BaseBlockDevice::is_attached() const
{
    return device_attached_vm.has_value();
}

bool mp::BaseBlockDevice::exists() const
{
    return QFile::exists(device_image_path);
}

void mp::BaseBlockDevice::remove_image_file()
{
    if (!QFile::remove(device_image_path))
        throw std::runtime_error(fmt::format("Failed to remove block device image: {}", 
                                            device_image_path.toStdString()));
}

void mp::BaseBlockDevice::validate_attach(const std::string& vm_name)
{
    if (is_attached())
        throw ValidationError(fmt::format("Block device '{}' is already attached to VM '{}'", 
                                         device_name, *device_attached_vm));
    
    if (!exists())
        throw NotFoundError(fmt::format("Block device '{}' image file does not exist: {}", 
                                       device_name, device_image_path.toStdString()));
}

void mp::BaseBlockDevice::validate_detach()
{
    if (!is_attached())
        throw ValidationError(fmt::format("Block device '{}' is not attached to any VM", device_name));
}
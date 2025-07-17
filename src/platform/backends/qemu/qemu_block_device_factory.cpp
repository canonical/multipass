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
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/utils.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

mp::BlockDevice::UPtr mp::QemuBlockDeviceFactory::create_block_device_from_file(const std::string& name,
                                                                                 const std::string& source_path,
                                                                                 const Path& data_dir)
{
    validate_name(name);
    
    // Convert source_path to QString for Qt operations
    QString source_qpath = QString::fromStdString(source_path);
    
    // Check if source file exists
    QFileInfo source_info(source_qpath);
    if (!source_info.exists())
        throw ValidationError(fmt::format("Source disk image file '{}' does not exist", source_path));
    
    if (!source_info.isFile())
        throw ValidationError(fmt::format("Source path '{}' is not a file", source_path));
    
    // Get image info using qemu-img
    QStringList qemuimg_parameters{{"info", source_qpath}};
    auto qemuimg_process = mp::platform::make_process(
        std::make_unique<mp::QemuImgProcessSpec>(qemuimg_parameters, source_qpath));
    auto process_state = qemuimg_process->execute();
    
    if (!process_state.completed_successfully())
    {
        throw ValidationError(
            fmt::format("Cannot get image info for '{}': qemu-img failed ({}) with output:\n{}",
                        source_path,
                        process_state.failure_message(),
                        qemuimg_process->read_all_standard_error()));
    }
    
    const auto img_info = QString{qemuimg_process->read_all_standard_output()};
    
    // Extract virtual size
    const auto size_pattern = QStringLiteral("^virtual size: .+ \\((?<size>\\d+) bytes\\)\r?$");
    const auto size_re = QRegularExpression{size_pattern, QRegularExpression::MultilineOption};
    const auto size_match = size_re.match(img_info);
    
    if (!size_match.hasMatch())
        throw ValidationError(fmt::format("Could not determine virtual size of disk image '{}'", source_path));
    
    mp::MemorySize image_size{size_match.captured("size").toStdString()};
    
    // Extract format
    const auto format_pattern = QStringLiteral("^file format: (?<format>\\w+)\r?$");
    const auto format_re = QRegularExpression{format_pattern, QRegularExpression::MultilineOption};
    const auto format_match = format_re.match(img_info);
    
    std::string format = "qcow2"; // default
    if (format_match.hasMatch())
        format = format_match.captured("format").toStdString();
    
    // Determine target path
    auto target_path = get_block_device_path(name, data_dir);
    
    // If source and target are the same, just create the block device object
    if (QFileInfo(source_qpath).canonicalFilePath() == QFileInfo(target_path).canonicalFilePath())
    {
        return std::make_unique<QemuBlockDevice>(name, target_path, image_size, format);
    }
    
    // Copy the source file to the target location
    QFileInfo target_info(target_path);
    if (target_info.exists())
        throw ValidationError(fmt::format("Block device '{}' already exists", name));
    
    // Ensure target directory exists
    QDir target_dir = target_info.dir();
    if (!target_dir.exists())
        target_dir.mkpath(".");
    
    if (!QFile::copy(source_qpath, target_path))
        throw std::runtime_error(fmt::format("Failed to copy disk image from '{}' to '{}'",
                                            source_path, target_path.toStdString()));
    
    return std::make_unique<QemuBlockDevice>(name, target_path, image_size, format);
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
    // Store block device images directly in the data directory (e.g., extra-disks/)
    // instead of creating nested block-devices/images/ subdirectories
    
    // Ensure the data directory exists
    MP_UTILS.make_dir(data_dir);
    
    return data_dir + "/" + QString::fromStdString(name) + ".qcow2";
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
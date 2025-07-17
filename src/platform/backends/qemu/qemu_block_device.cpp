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

#include "qemu_block_device.h"

#include <shared/qemu_img_utils/qemu_img_utils.h>

#include <multipass/format.h>
#include <multipass/process/qemuimg_process_spec.h>

namespace mp = multipass;

mp::QemuBlockDevice::QemuBlockDevice(const std::string& name,
                                     const Path& image_path,
                                     const MemorySize& size,
                                     const std::string& format,
                                     const std::optional<std::string>& attached_vm)
    : BaseBlockDevice(name, image_path, size, format, attached_vm)
{
}

void mp::QemuBlockDevice::create_image_file(const std::string& name,
                                            const MemorySize& size,
                                            const Path& image_path)
{
    // Create QCOW2 image using qemu-img
    auto process_spec = std::make_unique<QemuImgProcessSpec>(
        QStringList{"create", "-f", "qcow2", image_path, QString::number(size.in_bytes())},
        "",
        image_path);

    mp::backend::checked_exec_qemu_img(std::move(process_spec),
                                       fmt::format("Failed to create block device '{}'", name));
}

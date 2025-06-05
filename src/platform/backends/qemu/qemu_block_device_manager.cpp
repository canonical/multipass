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

#include "qemu_block_device_manager.h"

#include <shared/qemu_img_utils/qemu_img_utils.h>

#include <multipass/format.h>
#include <multipass/process/qemuimg_process_spec.h>

namespace mp = multipass;

using namespace multipass;

mp::QemuBlockDeviceManager::QemuBlockDeviceManager(const Path& data_dir)
    : BaseBlockDeviceManager(data_dir)
{
}

void mp::QemuBlockDeviceManager::create_block_device_image(const std::string& name, const MemorySize& size, const Path& image_path)
{
    // Create QCOW2 image using qemu-img
    auto process_spec = std::make_unique<QemuImgProcessSpec>(
        QStringList{"create", "-f", "qcow2", image_path, QString::number(size.in_bytes())},
        "",
        image_path);
    
    mp::backend::checked_exec_qemu_img(std::move(process_spec),
                                     fmt::format("Failed to create block device '{}'", name));
}
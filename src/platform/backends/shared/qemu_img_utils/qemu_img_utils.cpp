/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "qemu_img_utils.h"

#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace mp = multipass;

void mp::backend::resize_instance_image(const MemorySize& disk_space, const mp::Path& image_path)
{
    auto disk_size = QString::number(disk_space.in_bytes()); // format documented in `man qemu-img` (look for "size")
    QStringList qemuimg_parameters{{"resize", image_path, disk_size}};
    auto qemuimg_process =
        mp::platform::make_process(std::make_unique<mp::QemuImgProcessSpec>(qemuimg_parameters, "", image_path));

    auto process_state = qemuimg_process->execute(mp::image_resize_timeout);
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Cannot resize instance image: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(),
                                             qemuimg_process->read_all_standard_error()));
    }
}

mp::Path mp::backend::convert_to_qcow_if_necessary(const mp::Path& image_path)
{
    // Check if raw image file, and if so, convert to qcow2 format.
    // TODO: we could support converting from other the image formats that qemu-img can deal with
    const auto qcow2_path{image_path + ".qcow2"};

    auto qemuimg_info_spec =
        std::make_unique<mp::QemuImgProcessSpec>(QStringList{"info", "--output=json", image_path}, image_path);
    auto qemuimg_info_process = mp::platform::make_process(std::move(qemuimg_info_spec));

    auto process_state = qemuimg_info_process->execute();
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Cannot read image format: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(),
                                             qemuimg_info_process->read_all_standard_error()));
    }

    auto image_info = qemuimg_info_process->read_all_standard_output();
    auto image_record = QJsonDocument::fromJson(QString(image_info).toUtf8(), nullptr).object();

    if (image_record["format"].toString() == "raw")
    {
        auto qemuimg_convert_spec = std::make_unique<mp::QemuImgProcessSpec>(
            QStringList{"convert", "-p", "-O", "qcow2", image_path, qcow2_path}, image_path, qcow2_path);
        auto qemuimg_convert_process = mp::platform::make_process(std::move(qemuimg_convert_spec));
        process_state = qemuimg_convert_process->execute(mp::image_resize_timeout);

        if (!process_state.completed_successfully())
        {
            throw std::runtime_error(
                fmt::format("Failed to convert image format: qemu-img failed ({}) with output:\n{}",
                            process_state.failure_message(), qemuimg_convert_process->read_all_standard_error()));
        }
        return qcow2_path;
    }
    else
    {
        return image_path;
    }
}

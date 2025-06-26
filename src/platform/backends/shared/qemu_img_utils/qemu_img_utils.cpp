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

#include "qemu_img_utils.h"

#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace mp = multipass;
namespace mpp = multipass::platform;

auto mp::backend::checked_exec_qemu_img(std::unique_ptr<mp::QemuImgProcessSpec> spec,
                                        const std::string& custom_error_prefix,
                                        std::optional<int> timeout) -> Process::UPtr
{
    auto process = mpp::make_process(std::move(spec));

    auto process_state = timeout ? process->execute(*timeout) : process->execute();
    if (!process_state.completed_successfully())
    {
        throw QemuImgException{fmt::format("{}: qemu-img failed ({}) with output:\n{}",
                                           custom_error_prefix,
                                           process_state.failure_message(),
                                           process->read_all_standard_error())};
    }

    return process;
}

void mp::backend::resize_instance_image(const MemorySize& disk_space, const QString& image_path)
{
    auto disk_size = QString::number(
        disk_space.in_bytes()); // format documented in `man qemu-img` (look for "size")
    QStringList qemuimg_parameters{{"resize", image_path, disk_size}};

    checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(qemuimg_parameters, "", image_path),
        "Cannot resize instance image",
        mp::image_resize_timeout);
}

QString mp::backend::convert_to_qcow_if_necessary(const QString& image_path)
{
    // Check if raw image file, and if so, convert to qcow2 format.
    // TODO: we could support converting from other the image formats that qemu-img can deal with
    const auto qcow2_path{image_path + ".qcow2"};

    auto qemuimg_info_process = checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(QStringList{"info", "--output=json", image_path},
                                                 image_path),
        "Cannot read image format");

    auto image_info = qemuimg_info_process->read_all_standard_output();
    auto image_record = QJsonDocument::fromJson(QString(image_info).toUtf8(), nullptr).object();

    if (image_record["format"].toString() == "raw")
    {
        auto qemuimg_convert_spec = std::make_unique<mp::QemuImgProcessSpec>(
            QStringList{"convert", "-p", "-O", "qcow2", image_path, qcow2_path},
            image_path,
            qcow2_path);
        auto qemuimg_convert_process = checked_exec_qemu_img(std::move(qemuimg_convert_spec),
                                                             "Failed to convert image format");
        return qcow2_path;
    }
    else
    {
        return image_path;
    }
}

void mp::backend::amend_to_qcow2_v3(const QString& image_path)
{
    checked_exec_qemu_img(std::make_unique<mp::QemuImgProcessSpec>(
                              QStringList{"amend", "-o", "compat=1.1", image_path},
                              image_path),
                          "Failed to amend image to QCOW2 v3");
}

bool mp::backend::instance_image_has_snapshot(const QString& image_path, QString snapshot_tag)
{
    QRegularExpression regex{snapshot_tag.append(R"(\s)")};
    return QString{snapshot_list_output(image_path)}.contains(regex);
}

QByteArray mp::backend::snapshot_list_output(const QString& image_path)
{
    auto qemuimg_info_process = checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(QStringList{"snapshot", "-l", image_path},
                                                 image_path),
        "Cannot list snapshots from the image");
    return qemuimg_info_process->read_all_standard_output();
}

void mp::backend::delete_snapshot_from_image(const QString& image_path, const QString& snapshot_tag)
{
    checked_exec_qemu_img(std::make_unique<mp::QemuImgProcessSpec>(
                              QStringList{"snapshot", "-d", snapshot_tag, image_path},
                              image_path),
                          "Cannot delete snapshot from the image");
}

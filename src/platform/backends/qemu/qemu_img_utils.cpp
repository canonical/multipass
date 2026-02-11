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
#include <multipass/json_utils.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <boost/json.hpp>

namespace mp = multipass;
namespace mpp = multipass::platform;

QString mp::backend::get_image_info(const std::filesystem::path& image_path, const QString& key)
{
    auto qemuimg_info_process = checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(
            QStringList{"info", "--output=json", MP_PLATFORM.path_to_qstr(image_path)},
            image_path),
        "Cannot read image info");

    auto image_info = qemuimg_info_process->read_all_standard_output();
    auto image_record = boost::json::parse(std::string_view(image_info));
    return mp::lookup_or<QString>(image_record, key.toStdString(), "");
}

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

void mp::backend::resize_instance_image(const MemorySize& disk_space,
                                        const std::filesystem::path& image_path)
{
    // Detect the image format first to avoid qemu-img warnings and restrictions
    const auto image_format = get_image_info(image_path, "format");

    auto disk_size = QString::number(
        disk_space.in_bytes()); // format documented in `man qemu-img` (look for "size")
    QStringList qemuimg_parameters{
        {"resize", "-f", image_format, MP_PLATFORM.path_to_qstr(image_path), disk_size}};

    checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(qemuimg_parameters, "", image_path),
        "Cannot resize instance image",
        mp::image_resize_timeout);
}

void mp::backend::amend_to_qcow2_v3(const std::filesystem::path& image_path)
{
    checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(
            QStringList{"amend", "-o", "compat=1.1", MP_PLATFORM.path_to_qstr(image_path)},
            image_path),
        "Failed to amend image to QCOW2 v3");
}

std::filesystem::path mp::backend::convert(const std::filesystem::path& source_path,
                                           const std::string& target_format)
{
    auto target_img_path{source_path};
    target_img_path.replace_extension(target_format);

    const auto image_format = get_image_info(source_path, "format");
    if (image_format == target_format)
        return source_path; // no-op

    auto qemuimg_convert_spec = std::make_unique<mp::QemuImgProcessSpec>(
        QStringList{"convert",
                    "-p",
                    "-O",
                    QString::fromStdString(target_format),
                    MP_PLATFORM.path_to_qstr(source_path),
                    MP_PLATFORM.path_to_qstr(target_img_path)},
        source_path,
        target_img_path);

    auto qemuimg_convert_process =
        checked_exec_qemu_img(std::move(qemuimg_convert_spec), "Failed to convert image format");

    return target_img_path;
}

bool mp::backend::instance_image_has_snapshot(const std::filesystem::path& image_path,
                                              QString snapshot_tag)
{
    QRegularExpression regex{snapshot_tag.append(R"(\s)")};
    return QString{snapshot_list_output(image_path)}.contains(regex);
}

QByteArray mp::backend::snapshot_list_output(const std::filesystem::path& image_path)
{
    auto qemuimg_info_process = checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(
            QStringList{"snapshot", "-l", MP_PLATFORM.path_to_qstr(image_path)},
            image_path),
        "Cannot list snapshots from the image");
    return qemuimg_info_process->read_all_standard_output();
}

void mp::backend::delete_snapshot_from_image(const std::filesystem::path& image_path,
                                             const QString& snapshot_tag)
{
    checked_exec_qemu_img(
        std::make_unique<mp::QemuImgProcessSpec>(
            QStringList{"snapshot", "-d", snapshot_tag, MP_PLATFORM.path_to_qstr(image_path)},
            image_path),
        "Cannot delete snapshot from the image");
}

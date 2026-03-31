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

/*
 * This file provides some support for creating and resizing ASIF disk images.
 * There is no official public documentation for the ASIF format. The technical
 * details used here are based on community reverse engineering. For reference,
 * see: https://github.com/huven/asif-format
 */

#include "applevz_utils.h"

#include <applevz/applevz_wrapper.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/utils/qemu_img_utils.h>
#include <shared/macos/process_factory.h>

#include <Foundation/Foundation.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "applevz-utils";

std::string run_process(const QString& program, const QStringList& args, const std::string& desc)
{
    mpl::info(category, "Trying to {}", desc);

    auto process = MP_PROCFACTORY.create_process(program, args);

    if (const auto exit_state = process->execute(); !exit_state.completed_successfully())
        throw std::runtime_error(fmt::format("Failed to {}: {}; Output: {}",
                                             desc,
                                             exit_state.failure_message(),
                                             process->read_all_standard_error()));

    return process->read_all_standard_output().trimmed().toStdString();
}

bool is_asif_image(const std::filesystem::path& image_path)
{
    // ASIF format uses "shdw" magic bytes (0x73686477)
    const auto file = MP_FILEOPS.open_read(image_path, std::ios::binary);
    file->exceptions(std::ios::badbit);
    if (file->fail())
        throw std::runtime_error(fmt::format("Failed to open file for reading: {}", image_path));

    std::array<char, 4> magic{};
    file->read(magic.data(), magic.size());

    return file->gcount() == 4 && std::equal(magic.begin(), magic.end(), "shdw");
}

void resize_asif_image(const std::filesystem::path& image_path, const mp::MemorySize& disk_space)
{
    run_process(
        QStringLiteral("diskutil"),
        QStringList() << "image" << "resize" << "--size" << QString::number(disk_space.in_bytes())
                      << MP_PLATFORM.path_to_qstr(image_path),
        fmt::format("resize ASIF image: {}, size: {}", image_path, disk_space.human_readable()));
}

void create_asif_from(const std::filesystem::path& source_path,
                      const std::filesystem::path& dest_path)
{
    run_process(QStringLiteral("diskutil"),
                QStringList() << "image" << "create" << "from"
                              << MP_PLATFORM.path_to_qstr(source_path)
                              << MP_PLATFORM.path_to_qstr(dest_path) << "-f"
                              << "asif",
                fmt::format("convert {} to ASIF format at {}", source_path, dest_path));
}

void make_sparse(const std::filesystem::path& raw_image_path, const mp::MemorySize& disk_space)
{
    std::error_code ec;
    std::filesystem::resize_file(raw_image_path, disk_space.in_bytes(), ec);

    if (ec)
        throw std::runtime_error(fmt::format("Failed to resize file: {}", ec.message()));
}

std::filesystem::path convert_to_asif(const std::filesystem::path& source_path, bool destructive)
{
    if (is_asif_image(source_path))
        return source_path;

    mpl::info(category, "Converting {} to ASIF format", source_path);

    // NO-OP if already RAW
    auto raw_path = mp::backend::convert(source_path, "raw");

    auto asif_path = std::filesystem::path(source_path).replace_extension("asif");

    try
    {
        create_asif_from(raw_path, asif_path);
    }
    catch (...)
    {
        if (destructive)
            QFile::remove(raw_path);
        QFile::remove(asif_path);
        throw;
    }

    // This is often an intermediate file so we remove it, unless it came from an existing VM
    if (destructive)
        QFile::remove(raw_path);
    return asif_path;
}
} // namespace

namespace multipass::applevz
{
std::filesystem::path AppleVZUtils::convert_to_supported_format(
    const std::filesystem::path& image_path,
    bool destructive) const
{
    return macos_at_least(26, 0) ? convert_to_asif(image_path, destructive)
                                 : backend::convert(image_path, "raw");
}

void AppleVZUtils::resize_image(const MemorySize& disk_space,
                                const std::filesystem::path& image_path) const
{
    mpl::trace(category, "Resizing image to: {}", disk_space.human_readable());

    is_asif_image(image_path) ? resize_asif_image(image_path, disk_space)
                              : make_sparse(image_path, disk_space);

    mpl::trace(category, "Successfully resized image: {}", image_path);
}

bool AppleVZUtils::macos_at_least(int major, int minor, int patch) const
{
    NSOperatingSystemVersion v{major, minor, patch};
    return [[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:v];
}
} // namespace multipass::applevz

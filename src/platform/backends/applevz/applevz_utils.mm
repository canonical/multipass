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

void create_asif(const std::filesystem::path& image_path, const mp::MemorySize& disk_space)
{
    run_process(QStringLiteral("diskutil"),
                QStringList() << "image" << "create" << "blank" << "--fs" << "none" << "--format"
                              << "ASIF" << "--size" << QString::number(disk_space.in_bytes())
                              << MP_PLATFORM.path_to_qstr(image_path),
                fmt::format("create ASIF image: {}, size: {}",
                            image_path,
                            disk_space.human_readable()));
}

std::filesystem::path attach_asif(const std::filesystem::path& image_path)
{
    return run_process(QStringLiteral("diskutil"),
                       QStringList() << "image" << "attach" << "--noMount" << MP_PLATFORM.path_to_qstr(image_path),
                       fmt::format("attach ASIF image: {}", image_path));
}

void detach_asif(const std::filesystem::path& device_path)
{
    run_process(QStringLiteral("hdiutil"),
                QStringList() << "detach" << MP_PLATFORM.path_to_qstr(device_path),
                fmt::format("detach ASIF image: {}", device_path));
}

bool is_asif_image(const std::filesystem::path& image_path)
{
    // ASIF format uses "shdw" magic bytes (0x73686477)
    std::ifstream file(image_path, std::ios::binary);
    if (!file)
        return false;

    std::array<char, 4> magic{};
    file.read(magic.data(), magic.size());
    if (file.gcount() != 4)
        return false;

    return std::equal(magic.begin(), magic.end(), "shdw");
}

void resize_asif_image(const std::filesystem::path& image_path, const mp::MemorySize& disk_space)
{
    run_process(QStringLiteral("diskutil"),
                QStringList() << "image" << "resize" << "--size"
                              << QString::number(disk_space.in_bytes()) << MP_PLATFORM.path_to_qstr(image_path),
                fmt::format("resize ASIF image: {}, size: {}",
                            image_path,
                            disk_space.human_readable()));
}

void make_sparse(const std::filesystem::path& raw_image_path, const mp::MemorySize& disk_space)
{
    std::error_code ec;
    std::filesystem::resize_file(raw_image_path, disk_space.in_bytes(), ec);

    if (ec)
        throw std::runtime_error(fmt::format("Failed to resize file: {}", ec.message()));
}

std::filesystem::path convert_to_asif(const std::filesystem::path& source_path)
{
    if (is_asif_image(source_path))
        return source_path;

    constexpr size_t buffer_size = 1024 * 1024;

    mpl::info(category, "Converting {} to ASIF format", source_path);

    // NO-OP if already RAW
    auto raw_path = mp::backend::convert(source_path, "raw");

    std::error_code ec;
    const auto source_size = std::filesystem::file_size(raw_path, ec);
    if (ec)
        throw std::runtime_error(fmt::format("Failed to stat source image: {}", ec.message()));

    auto asif_path = std::filesystem::path(source_path).replace_extension("asif");
    create_asif(asif_path, mp::MemorySize{std::to_string(source_size)});

    auto device_path = attach_asif(asif_path);
    try
    {
        {
            std::ifstream source(raw_path, std::ios::binary);
            if (!source)
                throw std::runtime_error(fmt::format("Failed to open source image: {}", raw_path));

            std::ofstream target(device_path, std::ios::binary);
            if (!target)
                throw std::runtime_error(
                    fmt::format("Failed to open target device: {}", device_path));

            std::vector<char> buffer(buffer_size);
            while (source.read(buffer.data(), buffer_size) || source.gcount())
            {
                const auto bytes_read = source.gcount();
                const bool is_zero = std::all_of(buffer.cbegin(),
                                                 buffer.cbegin() + bytes_read,
                                                 [](char c) { return c == '\0'; });

                is_zero ? target.seekp(bytes_read, std::ios::cur)
                        : target.write(buffer.data(), bytes_read);
            }
        }

        mpl::info(category, "Successfully converted to ASIF format");

        detach_asif(device_path);
        std::filesystem::remove(raw_path);

        return asif_path;
    }
    catch (...)
    {
        detach_asif(device_path);
        std::filesystem::remove(raw_path);
        throw;
    }
}
} // namespace

namespace multipass::applevz
{
std::filesystem::path AppleVZUtils::convert_to_supported_format(
    const std::filesystem::path& image_path) const
{
    return macos_at_least(26, 0) ? convert_to_asif(image_path)
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

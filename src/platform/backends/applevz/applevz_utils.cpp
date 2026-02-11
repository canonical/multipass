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

#include "applevz_utils.h"

#include <applevz/applevz_wrapper.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <qemu/qemu_img_utils.h>
#include <shared/macos/process_factory.h>

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "applevz-utils";

void create_asif(const mp::Path& image_path, const mp::MemorySize& disk_space)
{
    mpl::info(category,
              "Creating ASIF image in directory: {}, with size: {}",
              image_path.toStdString(),
              disk_space.human_readable());

    auto process = MP_PROCFACTORY.create_process(
        QStringLiteral("diskutil"),
        QStringList() << "image" << "create" << "blank" << "--fs" << "none" << "--format" << "ASIF"
                      << "--size" << QString::number(disk_space.in_bytes()) << image_path);

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Failed to create ASIF image: {}; Output: {}",
                                             exit_state.failure_message(),
                                             process->read_all_standard_error().toStdString()));
    }

    mpl::trace(category, "Successfully created ASIF image: {}", image_path);
}

bool is_asif_image(const mp::Path& image_path)
{
    // ASIF format uses "shdw" magic bytes (0x73686477)
    int fd = open(image_path.toStdString().c_str(), O_RDONLY);
    if (fd == -1)
        return false;

    char magic[4];
    ssize_t bytes_read = read(fd, magic, 4);
    close(fd);

    if (bytes_read != 4)
        return false;

    return !std::memcmp(magic, "shdw", 4);
}

void make_sparse(const mp::Path& path, const mp::MemorySize& disk_space)
{
    int fd = open(path.toStdString().c_str(), O_RDWR);
    if (fd == -1)
    {
        throw std::runtime_error(
            fmt::format("Failed to open file for resizing: {}", strerror(errno)));
    }

    auto size_bytes = static_cast<off_t>(disk_space.in_bytes());
    if (ftruncate(fd, size_bytes) == -1)
    {
        close(fd);
        throw std::runtime_error(fmt::format("Failed to resize file: {}", strerror(errno)));
    }

    close(fd);
}

mp::Path convert_to_asif(const mp::Path& source_path)
{
    return source_path + ".asif";
}
} // namespace

namespace multipass::applevz
{
Path convert_to_supported_format(const Path& image_path)
{
    if (MP_APPLEVZ.macos_at_least(26, 0) && !is_asif_image(image_path))
    {
        return convert_to_asif(image_path);
    }
    else
    {
        return backend::convert_to_raw(image_path);
    }
}

void resize_image(const MemorySize& disk_space, const Path& image_path)
{
    mpl::trace(category, "Resizing image to: {}", disk_space.human_readable());

    if (is_asif_image(image_path))
    {
        auto process = MP_PROCFACTORY.create_process(
            QStringLiteral("diskutil"),
            QStringList() << "image" << "resize" << "--size"
                          << QString::number(disk_space.in_bytes()) << image_path);

        auto exit_state = process->execute();

        if (!exit_state.completed_successfully())
        {
            throw std::runtime_error(fmt::format("Failed to resize ASIF device: {}; Output: {}",
                                                 exit_state.failure_message(),
                                                 process->read_all_standard_error().toStdString()));
        }
    }
    else
    {
        make_sparse(image_path, disk_space);
    }

    mpl::trace(category, "Successfully resized image: {}", image_path);
}
} // namespace multipass::applevz

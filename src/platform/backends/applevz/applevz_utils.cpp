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
#include <multipass/utils/qemu_img_utils.h>
#include <shared/macos/process_factory.h>

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

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

mp::Path attach_asif(const mp::Path& image_path)
{
    mpl::info(category, "Attaching ASIF image: {}", image_path.toStdString());

    auto process = MP_PROCFACTORY.create_process(QStringLiteral("diskutil"),
                                                 QStringList() << "image" << "attach" << "--noMount"
                                                               << image_path);

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Failed to attach ASIF image: {}; Output: {}",
                                             exit_state.failure_message(),
                                             process->read_all_standard_error().toStdString()));
    }

    mpl::trace(category, "Successfully attached ASIF image: {}", image_path.toStdString());

    return process->read_all_standard_output().trimmed();
}

void detach_asif(const mp::Path& device_path)
{
    mpl::info(category, "Detaching ASIF image: {}", device_path.toStdString());

    auto process = MP_PROCFACTORY.create_process(QStringLiteral("hdiutil"),
                                                 QStringList() << "detach" << device_path);

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Failed to detach ASIF image: {}; Output: {}",
                                             exit_state.failure_message(),
                                             process->read_all_standard_error().toStdString()));
    }

    mpl::trace(category, "Successfully detached ASIF image: {}", device_path.toStdString());
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
    constexpr size_t buffer_size = 1024 * 1024;

    mpl::info(category, "Converting {} to ASIF format", source_path.toStdString());

    // NO-OP if already RAW
    mp::Path raw_path = mp::backend::convert(source_path, "raw");

    struct stat st;
    if (stat(raw_path.toStdString().c_str(), &st) == -1)
    {
        throw std::runtime_error(fmt::format("Failed to stat source image: {}", strerror(errno)));
    }
    mp::MemorySize source_size{std::to_string(st.st_size)};

    QFileInfo source_info(source_path);
    auto asif_path =
        QString("%1/%2.asif").arg(source_info.path()).arg(source_info.completeBaseName());
    create_asif(asif_path, source_size);

    mp::Path device_path = attach_asif(asif_path);
    try
    {
        int source_fd = open(raw_path.toStdString().c_str(), O_RDONLY);
        if (source_fd == -1)
        {
            throw std::runtime_error(fmt::format("Failed to open source image {}: {}",
                                                 raw_path.toStdString(),
                                                 strerror(errno)));
        }

        int target_fd = open(device_path.toStdString().c_str(), O_WRONLY);
        if (target_fd == -1)
        {
            close(source_fd);
            throw std::runtime_error(fmt::format("Failed to open target device {}: {}",
                                                 device_path.toStdString(),
                                                 strerror(errno)));
        }

        struct stat st;
        if (fstat(source_fd, &st) == -1)
        {
            close(source_fd);
            close(target_fd);
            throw std::runtime_error(
                fmt::format("Failed to stat source image: {}", strerror(errno)));
        }

        off_t total_size = st.st_size;
        off_t bytes_copied = 0;

        std::vector<char> buffer(buffer_size);
        std::vector<char> zero_buffer(buffer_size, 0);

        while (bytes_copied < total_size)
        {
            size_t bytes_to_read =
                std::min(static_cast<size_t>(total_size - bytes_copied), buffer_size);

            ssize_t bytes_read = read(source_fd, buffer.data(), bytes_to_read);
            if (bytes_read == -1)
            {
                close(source_fd);
                close(target_fd);
                throw std::runtime_error(
                    fmt::format("Failed to read from source: {}", strerror(errno)));
            }

            if (bytes_read == 0)
                break;

            // Check if block is all zeros
            bool is_zero = std::memcmp(buffer.data(), zero_buffer.data(), bytes_read) == 0;

            if (!is_zero)
            {
                // Write non-zero data
                ssize_t bytes_written = write(target_fd, buffer.data(), bytes_read);
                if (bytes_written == -1)
                {
                    close(source_fd);
                    close(target_fd);
                    throw std::runtime_error(
                        fmt::format("Failed to write to target: {}", strerror(errno)));
                }

                if (bytes_written != bytes_read)
                {
                    close(source_fd);
                    close(target_fd);
                    throw std::runtime_error(fmt::format("Short write: read {} bytes but wrote {}",
                                                         bytes_read,
                                                         bytes_written));
                }
            }
            else
            {
                // Skip zero blocks by seeking forward
                if (lseek(target_fd, bytes_read, SEEK_CUR) == -1)
                {
                    close(source_fd);
                    close(target_fd);
                    throw std::runtime_error(
                        fmt::format("Failed to seek in target: {}", strerror(errno)));
                }
            }

            bytes_copied += bytes_read;
        }

        close(source_fd);
        close(target_fd);

        mpl::info(category, "Successfully converted {} bytes to ASIF format", bytes_copied);

        detach_asif(device_path);
        QFile::remove(raw_path);

        return asif_path;
    }
    catch (...)
    {
        detach_asif(device_path);
        QFile::remove(asif_path);
        throw;
    }
}
} // namespace

namespace multipass::applevz
{
Path AppleVZImageUtils::convert_to_supported_format(const Path& image_path) const
{
    if (MP_APPLEVZ.macos_at_least(26, 0) && !is_asif_image(image_path))
    {
        return convert_to_asif(image_path);
    }
    else
    {
        return backend::convert(image_path, "raw");
    }
}

void AppleVZImageUtils::resize_image(const MemorySize& disk_space, const Path& image_path) const
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

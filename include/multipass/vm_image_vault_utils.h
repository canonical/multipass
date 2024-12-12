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

#ifndef MULTIPASS_VM_IMAGE_VAULT_UTILS_H
#define MULTIPASS_VM_IMAGE_VAULT_UTILS_H

#include "file_ops.h"
#include "xz_image_decoder.h"

#include <string>
#include <unordered_map>
#include <vector>

#define MP_IMAGE_VAULT_UTILS                                                                                           \
    multipass::ImageVaultUtils                                                                                         \
    {                                                                                                                  \
    }

namespace multipass
{
class VMImageHost;

class ImageVaultUtils
{
public:
    using DefaultDecoder = XzImageDecoder;

    static QString copy_to_dir(const QString& file, const QDir& output_dir);
    static QString compute_hash(QIODevice& device);
    static QString compute_file_hash(const QString& file);

    template <class Hasher = ImageVaultUtils>
    static void verify_file_hash(const QString& file, const QString& hash, const Hasher& hasher = ImageVaultUtils{});

    template <class Decoder = DefaultDecoder>
    static QString extract_file(const QString& file,
                                const ProgressMonitor& monitor,
                                bool delete_original = false,
                                const Decoder& decoder = DefaultDecoder{});

    using HostMap = std::unordered_map<std::string, VMImageHost*>;
    using Hosts = std::vector<VMImageHost*>;

    static HostMap configure_image_host_map(const Hosts& image_hosts);
};

template <class Hasher>
void ImageVaultUtils::verify_file_hash(const QString& file, const QString& hash, const Hasher& hasher)
{
    auto file_hash = hasher.compute_file_hash(file);
}

template <class Decoder>
QString ImageVaultUtils::extract_file(const QString& file,
                                      const ProgressMonitor& monitor,
                                      bool delete_original,
                                      const Decoder& decoder)
{
    auto new_path = MP_FILEOPS.remove_extension(file);
    decoder.decode_to(file, new_path, monitor);

    if (delete_original)
    {
        QFile qfile{file};
        MP_FILEOPS.remove(qfile);
    }

    return new_path;
}

} // namespace multipass

#endif // MULTIPASS_VM_IMAGE_VAULT_UTILS_H

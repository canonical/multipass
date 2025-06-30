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

#include <multipass/format.h>
#include <multipass/image_host/vm_image_host.h>
#include <multipass/vm_image_vault.h>
#include <multipass/vm_image_vault_utils.h>
#include <multipass/xz_image_decoder.h>

#include <QCryptographicHash>
#include <QFileInfo>

#include <stdexcept>

namespace mp = multipass;

mp::ImageVaultUtils::ImageVaultUtils(const PrivatePass& pass) noexcept : Singleton{pass}
{
}

QString mp::ImageVaultUtils::copy_to_dir(const QString& file, const QDir& output_dir) const
{
    if (file.isEmpty())
        return "";

    const QFileInfo info{file};
    if (!MP_FILEOPS.exists(info))
        throw std::runtime_error(fmt::format("File {} not found", file));

    auto new_location = output_dir.filePath(info.fileName());

    if (!MP_FILEOPS.copy(file, new_location))
        throw std::runtime_error(fmt::format("Failed to copy {} to {}", file, new_location));

    return new_location;
}

QString mp::ImageVaultUtils::compute_hash(QIODevice& device) const
{
    QCryptographicHash hash{QCryptographicHash::Sha256};
    if (!hash.addData(std::addressof(device)))
        throw std::runtime_error("Failed to read data from device to hash");

    return hash.result().toHex();
}

QString mp::ImageVaultUtils::compute_file_hash(const QString& path) const
{
    QFile file{path};
    if (!MP_FILEOPS.open(file, QFile::ReadOnly))
        throw std::runtime_error(fmt::format("Failed to open {}", path));

    return compute_hash(file);
}

void mp::ImageVaultUtils::verify_file_hash(const QString& file, const QString& hash) const
{
    const auto file_hash = compute_file_hash(file);

    if (file_hash != hash)
        throw std::runtime_error(fmt::format("Hash of {} does not match {}", file, hash));
}

QString mp::ImageVaultUtils::extract_file(const QString& file,
                                          const Decoder& decoder,
                                          bool delete_original) const
{
    const auto fs_new_path = MP_FILEOPS.remove_extension(file.toStdU16String());
    auto new_path = QString::fromStdString(fs_new_path.string());

    decoder(file, new_path);

    if (delete_original)
    {
        QFile qfile{file};
        MP_FILEOPS.remove(qfile);
    }

    return new_path;
}

mp::ImageVaultUtils::HostMap mp::ImageVaultUtils::configure_image_host_map(
    const Hosts& image_hosts) const
{
    HostMap remote_image_host_map{};
    for (const auto& image_host : image_hosts)
    {
        for (const auto& remote : image_host->supported_remotes())
        {
            remote_image_host_map[remote] = image_host;
        }
    }

    return remote_image_host_map;
}

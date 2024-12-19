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
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>
#include <multipass/xz_image_decoder.h>

#include <QCryptographicHash>
#include <QFileInfo>

#include <stdexcept>

namespace mp = multipass;

QString mp::vault::filename_for(const mp::Path& path)
{
    QFileInfo file_info(path);
    return file_info.fileName();
}

QString mp::vault::copy(const QString& file_name, const QDir& output_dir)
{
    if (file_name.isEmpty())
        return {};

    if (!QFileInfo::exists(file_name))
        throw std::runtime_error(fmt::format("{} missing", file_name));

    QFileInfo info{file_name};
    const auto source_name = info.fileName();
    auto new_path = output_dir.filePath(source_name);
    QFile::copy(file_name, new_path);
    return new_path;
}

void mp::vault::delete_file(const mp::Path& path)
{
    QFile file{path};
    file.remove();
}

QString mp::vault::compute_image_hash(const mp::Path& image_path)
{
    QFile image_file(image_path);
    if (!image_file.open(QFile::ReadOnly))
    {
        throw std::runtime_error("Cannot open image file for computing hash");
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&image_file))
    {
        throw std::runtime_error("Cannot read image file to compute hash");
    }

    return hash.result().toHex();
}

void mp::vault::verify_image_download(const mp::Path& image_path, const QString& image_hash)
{
    auto computed_hash = compute_image_hash(image_path);

    if (computed_hash != image_hash)
    {
        throw std::runtime_error("Downloaded image hash does not match");
    }
}

QString mp::vault::extract_image(const mp::Path& image_path, const mp::ProgressMonitor& monitor, const bool delete_file)
{
    mp::XzImageDecoder xz_decoder(image_path);
    QString new_image_path{image_path};

    new_image_path.remove(".xz");

    xz_decoder.decode_to(new_image_path, monitor);

    mp::vault::delete_file(image_path);

    return new_image_path;
}

std::unordered_map<std::string, mp::VMImageHost*>
mp::vault::configure_image_host_map(const std::vector<mp::VMImageHost*>& image_hosts)
{
    std::unordered_map<std::string, mp::VMImageHost*> remote_image_host_map;

    for (const auto& image_host : image_hosts)
    {
        for (const auto& remote : image_host->supported_remotes())
        {
            remote_image_host_map[remote] = image_host;
        }
    }

    return remote_image_host_map;
}

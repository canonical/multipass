/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include <multipass/vm_image_vault.h>
#include <multipass/xz_image_decoder.h>

#include <QCryptographicHash>
#include <QFileInfo>

namespace mp = multipass;

QString mp::vault::filename_for(const mp::Path& path)
{
    QFileInfo file_info(path);
    return file_info.fileName();
}

void mp::vault::delete_file(const mp::Path& path)
{
    QFile file{path};
    file.remove();
}

void mp::vault::verify_image_download(const mp::Path& image_path, const QString& image_hash)
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

    if (hash.result().toHex() != image_hash)
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

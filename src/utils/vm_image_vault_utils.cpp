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
#include <multipass/vm_image_vault_utils.h>
#include <multipass/xz_image_decoder.h>

#include <QCryptographicHash>
#include <QFileInfo>

#include <stdexcept>

namespace mp = multipass;

QString mp::ImageVaultUtils::copy_to_dir(const QString& file, const QDir& output_dir)
{
    return "";
}

QString mp::ImageVaultUtils::compute_hash(QIODevice& device)
{
    return "";
}

QString mp::ImageVaultUtils::compute_file_hash(const QString& path)
{
    return "";
}

mp::ImageVaultUtils::HostMap mp::ImageVaultUtils::configure_image_host_map(const Hosts& image_hosts)
{
    return {};
    /*
    std::unordered_map<std::string, mp::VMImageHost*> remote_image_host_map;

    for (const auto& image_host : image_hosts)
    {
        for (const auto& remote : image_host->supported_remotes())
        {
            remote_image_host_map[remote] = image_host;
        }
    }

    return remote_image_host_map;*/
}

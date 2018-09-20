/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "custom_image_host.h"

#include <multipass/query.h>
#include <multipass/url_downloader.h>

#include <QString>

namespace mp = multipass;

namespace
{
auto multipass_default_aliases(mp::URLDownloader* url_downloader)
{
    std::unordered_map<std::string, mp::VMImageInfo> default_aliases;
    const QString url{"http://cdimage.ubuntu.com/ubuntu-core/16/current/ubuntu-core-16-amd64.img.xz"};
    const auto last_modified = url_downloader->last_modified({url});
    const auto sha256_sums =
        url_downloader->download({"http://cdimage.ubuntu.com/ubuntu-core/16/current/SHA256SUMS"}).split('\n');
    QString hash;

    for (const auto& line : sha256_sums)
    {
        if (line.endsWith("ubuntu-core-16-amd64.img.xz"))
        {
            hash = QString(line.split(' ').first());
            break;
        }
    }

    mp::VMImageInfo core_image_info{
        {"core"}, "core-16", "Core 16", true, url, "", "", hash, last_modified.toString("yyyyMMdd"), 0};

    default_aliases.insert({"core", core_image_info});

    // Image information on 16.04
    const QString url_16{"https://cloud-images.ubuntu.com/releases/16.04/release/ubuntu-16.04-server-cloudimg-amd64-disk1.img"};
    const auto last_modified = url_downloader_16->last_modified({url_16});
    const auto sha256_sums_16 =
        url_downloader_16->download({"https://cloud-images.ubuntu.com/releases/16.04/release/SHA256SUMS"}).split('\n');
    QString hash_16;

    for (const auto& line : sha256_sums_16)
    {
        if (line.endsWith("ubuntu-16.04-server-cloudimg-amd64-disk1.img"))
        {
            hash = QString(line.split(' ').first());
            break;
        }
    }

    // Image information on 18.04
    const QString url_18{"https://cloud-images.ubuntu.com/releases/18.04/release/ubuntu-18.04-server-cloudimg-amd64.img"};
    const auto last_modified = url_downloader_18->last_modified({url_18});
    const auto sha256_sums_18 =
        url_downloader_18->download({"https://cloud-images.ubuntu.com/releases/18.04/release/SHA256SUMS"}).split('\n');
    QString hash_18;

    for (const auto& line : sha256_sums_18)
    {
        if (line.endsWith("ubuntu-18.04-server-cloudimg-amd64.img"))
        {
            hash = QString(line.split(' ').first());
            break;
        }
    }

    // snapcraft-core
    mp::VMImageInfo snapcraft_core_image_info{
        {"snapcraft-core"}, "snapcraft-core16", "snapcraft builder for core", true, url_16, "", "", hash, last_modified.toString("yyyyMMdd"), 0};

    default_aliases.insert({"snapcraft-core", snapcraft_core_image_info});

    // snapcraft-core16
    mp::VMImageInfo snapcraft_core16_image_info{
        {"snapcraft-core16"}, "snapcraft-core16", "snapcraft builder for core16", true, url_16, "", "", hash, last_modified.toString("yyyyMMdd"), 0};

    default_aliases.insert({"snapcraft-core16", snapcraft_core16_image_info});

    // snapcraft-core18
    mp::VMImageInfo snapcraft_core18_image_info{
        {"snapcraft-core18"}, "snapcraft-core18", "snapcraft builder for core18", true, url_18, "", "", hash, last_modified.toString("yyyyMMdd"), 0};

    default_aliases.insert({"snapcraft-core18", snapcraft_core18_image_info});

    return default_aliases;
}
} // namespace

mp::CustomVMImageHost::CustomVMImageHost(URLDownloader* downloader)
    : url_downloader{downloader}, custom_image_info{multipass_default_aliases(url_downloader)}
{
}

mp::optional<mp::VMImageInfo> mp::CustomVMImageHost::info_for(const Query& query)
{
    auto it = custom_image_info.find(query.release);

    if (it == custom_image_info.end())
        return {};

    return optional<VMImageInfo>{it->second};
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_info_for(const Query& query)
{
    std::vector<mp::VMImageInfo> images;

    auto it = custom_image_info.find(query.release);

    if (it == custom_image_info.end())
        return {};

    images.push_back(it->second);

    return images;
}

mp::VMImageInfo mp::CustomVMImageHost::info_for_full_hash(const std::string& full_hash)
{
    return {};
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_images_for(const std::string& remote_name)
{
    return {};
}

void mp::CustomVMImageHost::for_each_entry_do(const Action& action)
{
    for (const auto& info : custom_image_info)
    {
        action("", info.second);
    }
}

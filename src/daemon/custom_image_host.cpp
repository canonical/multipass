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
        if (line.contains("ubuntu-core-16-amd64.img.xz"))
        {
            hash = QString(line.split(' ').first());
            break;
        }
    }

    mp::VMImageInfo core_image_info{
        {"core"}, "core-16", "Core 16", true, url, "", "", hash, last_modified.toString("yyyyMMdd"), 0};

    default_aliases.insert({"core", core_image_info});

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

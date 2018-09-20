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

#include <fmt/format.h>

namespace mp = multipass;

namespace
{
struct BaseImageInfo
{
    const QString last_modified;
    const QString hash;
};

auto base_image_info_for(mp::URLDownloader* url_downloader, const QString& image_url, const QString& hash_url,
                         const QString& image_file)
{
    const auto last_modified = url_downloader->last_modified({image_url}).toString("yyyyMMdd");
    const auto sha256_sums = url_downloader->download({hash_url}).split('\n');
    QString hash;

    for (const auto& line : sha256_sums)
    {
        if (line.endsWith(image_file.toUtf8()))
        {
            hash = QString(line.split(' ').first());
            break;
        }
    }

    return BaseImageInfo{last_modified, hash};
}

auto map_aliases_to_vm_info_for(const std::vector<mp::VMImageInfo>& images)
{
    std::unordered_map<std::string, const mp::VMImageInfo*> map;
    for (const auto& image : images)
    {
        map[image.id.toStdString()] = &image;
        for (const auto& alias : image.aliases)
        {
            map[alias.toStdString()] = &image;
        }
    }

    return map;
}

auto multipass_default_aliases(mp::URLDownloader* url_downloader, const QString& path_prefix)
{
    std::vector<mp::VMImageInfo> default_images;
    QString image_url;
    QString hash_url;
    QString image_file{"ubuntu-core-16-amd64.img.xz"};

    if (!path_prefix.isEmpty())
    {
        image_url = QUrl::fromLocalFile(path_prefix + image_file).toString();
        hash_url = QUrl::fromLocalFile(path_prefix + "SHA256SUMS").toString();
    }
    else
    {
        image_url = "http://cdimage.ubuntu.com/ubuntu-core/16/current/" + image_file;
        hash_url = "http://cdimage.ubuntu.com/ubuntu-core/16/current/SHA256SUMS";
    }

    auto base_image_info = base_image_info_for(url_downloader, image_url, hash_url, image_file);
    mp::VMImageInfo core_image_info{{"core"},
                                    "Ubuntu",
                                    "core-16",
                                    "Core 16",
                                    true,
                                    image_url,
                                    "",
                                    "",
                                    base_image_info.hash,
                                    base_image_info.last_modified,
                                    0};

    default_images.push_back(core_image_info);

    auto map = map_aliases_to_vm_info_for(default_images);

    return std::unique_ptr<mp::CustomManifest>(new mp::CustomManifest{std::move(default_images), std::move(map)});
}

auto snapcraft_default_aliases(mp::URLDownloader* url_downloader, const QString& path_prefix)
{
    std::vector<mp::VMImageInfo> default_images;
    QString image_url;
    QString hash_url;
    QString image_file;

    {
        image_file = "ubuntu-16.04-server-cloudimg-amd64-disk1.img";
        if (!path_prefix.isEmpty())
        {
            image_url = QUrl::fromLocalFile(path_prefix + image_file).toString();
            hash_url = QUrl::fromLocalFile(path_prefix + "SHA256SUMS").toString();
        }
        else
        {
            image_url = "https://cloud-images.ubuntu.com/releases/16.04/release/" + image_file;
            hash_url = "https://cloud-images.ubuntu.com/releases/16.04/release/SHA256SUMS";
        }

        auto base_image_info = base_image_info_for(url_downloader, image_url, hash_url, image_file);
        default_images.push_back(mp::VMImageInfo{{"core", "core16"},
                                                 "",
                                                 "snapcraft-core16",
                                                 "Snapcraft builder for Core 16",
                                                 true,
                                                 image_url,
                                                 "",
                                                 "",
                                                 base_image_info.hash,
                                                 base_image_info.last_modified,
                                                 0});
    }

    {
        image_file = "ubuntu-18.04-server-cloudimg-amd64.img";
        if (!path_prefix.isEmpty())
        {
            image_url = QUrl::fromLocalFile(path_prefix + image_file).toString();
            hash_url = QUrl::fromLocalFile(path_prefix + "SHA256SUMS").toString();
        }
        else
        {
            image_url = "https://cloud-images.ubuntu.com/releases/18.04/release/" + image_file;
            hash_url = "https://cloud-images.ubuntu.com/releases/18.04/release/SHA256SUMS";
        }

        auto base_image_info = base_image_info_for(url_downloader, image_url, hash_url, image_file);
        default_images.push_back(mp::VMImageInfo{{"core18"},
                                                 "",
                                                 "snapcraft-core18",
                                                 "Snapcraft builder for Core 18",
                                                 true,
                                                 image_url,
                                                 "",
                                                 "",
                                                 base_image_info.hash,
                                                 base_image_info.last_modified,
                                                 0});
    }

    auto map = map_aliases_to_vm_info_for(default_images);

    return std::unique_ptr<mp::CustomManifest>(new mp::CustomManifest{std::move(default_images), std::move(map)});
}

auto custom_aliases(mp::URLDownloader* url_downloader, const QString& path_prefix)
{
    std::unordered_map<std::string, std::unique_ptr<mp::CustomManifest>> custom_manifests;

    custom_manifests.emplace(mp::no_remote, multipass_default_aliases(url_downloader, path_prefix));
    custom_manifests.emplace(mp::snapcraft_remote, snapcraft_default_aliases(url_downloader, path_prefix));

    return custom_manifests;
}
} // namespace

mp::CustomVMImageHost::CustomVMImageHost(URLDownloader* downloader) : CustomVMImageHost{downloader, ""}
{
}

mp::CustomVMImageHost::CustomVMImageHost(URLDownloader* downloader, const QString& path_prefix)
    : url_downloader{downloader},
      custom_image_info{custom_aliases(url_downloader, path_prefix)},
      remotes{no_remote, snapcraft_remote}
{
}

mp::optional<mp::VMImageInfo> mp::CustomVMImageHost::info_for(const Query& query)
{
    auto custom_manifest = manifest_from(query.remote_name);

    auto it = custom_manifest->image_records.find(query.release);

    if (it == custom_manifest->image_records.end())
        throw std::runtime_error(fmt::format("Unable to find an image matching \"{}\"", query.release));

    return *it->second;
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_info_for(const Query& query)
{
    std::vector<mp::VMImageInfo> images;

    images.push_back(*info_for(query));

    return images;
}

mp::VMImageInfo mp::CustomVMImageHost::info_for_full_hash(const std::string& full_hash)
{
    return {};
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_images_for(const std::string& remote_name)
{
    std::vector<mp::VMImageInfo> images;
    auto custom_manifest = manifest_from(remote_name);

    for (const auto& product : custom_manifest->products)
    {
        images.push_back(product);
    }

    return images;
}

void mp::CustomVMImageHost::for_each_entry_do(const Action& action)
{
    for (const auto& manifest : custom_image_info)
    {
        for (const auto& info : manifest.second->products)
        {
            action(manifest.first, info);
        }
    }
}

std::vector<std::string> mp::CustomVMImageHost::supported_remotes()
{
    return remotes;
}

mp::CustomManifest* mp::CustomVMImageHost::manifest_from(const std::string& remote_name)
{
    auto it = custom_image_info.find(remote_name);
    if (it == custom_image_info.end())
        throw std::runtime_error(fmt::format("Remote \"{}\" is unknown.", remote_name));

    return it->second.get();
}

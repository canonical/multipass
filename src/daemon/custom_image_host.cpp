/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>

#include <multipass/exceptions/download_exception.h>

#include <multipass/format.h>

#include <QMap>
#include <QUrl>

#include <utility>

namespace mp = multipass;

namespace
{
constexpr auto no_remote = "";
constexpr auto snapcraft_remote = "snapcraft";

struct BaseImageInfo
{
    const QString last_modified;
    const QString hash;
};

struct CustomImageInfo
{
    QString url_prefix;
    QStringList aliases;
    QString os;
    QString release;
    QString release_string;
    QString kernel_location;
    QString initrd_location;
};

const QMap<QString, CustomImageInfo> multipass_image_info{
    {{"ubuntu-core-16-amd64.img.xz"},
     {"http://cdimage.ubuntu.com/ubuntu-core/16/stable/current/",
      {"core", "core16"},
      "Ubuntu",
      "core-16",
      "Core 16",
      "",
      ""}},
    {{"ubuntu-core-18-amd64.img.xz"},
     {"http://cdimage.ubuntu.com/ubuntu-core/18/stable/current/", {"core18"}, "Ubuntu", "core-18", "Core 18", "", ""}}};

const QMap<QString, CustomImageInfo> snapcraft_image_info{
    {{"ubuntu-16.04-minimal-cloudimg-amd64-disk1.img"},
     {"https://cloud-images.ubuntu.com/minimal/releases/xenial/release/",
      {"core"},
      "",
      "snapcraft-core16",
      "Snapcraft builder for Core 16",
      "https://cloud-images.ubuntu.com/releases/xenial/release/unpacked/"
      "ubuntu-16.04-server-cloudimg-amd64-vmlinuz-generic",
      "https://cloud-images.ubuntu.com/releases/xenial/release/unpacked/"
      "ubuntu-16.04-server-cloudimg-amd64-initrd-generic"}},
    {{"bionic-server-cloudimg-amd64-disk.img"},
     {"https://cloud-images.ubuntu.com/buildd/releases/bionic/release/",
      {"core18"},
      "",
      "snapcraft-core18",
      "Snapcraft builder for Core 18",
      "https://cloud-images.ubuntu.com/releases/bionic/release/unpacked/"
      "ubuntu-18.04-server-cloudimg-amd64-vmlinuz-generic",
      "https://cloud-images.ubuntu.com/releases/bionic/release/unpacked/"
      "ubuntu-18.04-server-cloudimg-amd64-initrd-generic"}},
    {{"focal-server-cloudimg-amd64-disk.img"},
     {"https://cloud-images.ubuntu.com/buildd/releases/focal/release/",
      {"core20"},
      "",
      "snapcraft-core20",
      "Snapcraft builder for Core 20",
      "https://cloud-images.ubuntu.com/releases/focal/release/unpacked/"
      "ubuntu-20.04-server-cloudimg-amd64-vmlinuz-generic",
      "https://cloud-images.ubuntu.com/releases/focal/release/unpacked/"
      "ubuntu-20.04-server-cloudimg-amd64-initrd-generic"}}};

auto base_image_info_for(mp::URLDownloader* url_downloader, const QString& image_url, const QString& hash_url,
                         const QString& image_file)
{
    const auto last_modified = url_downloader->last_modified({image_url}).toString("yyyyMMdd");
    const auto sha256_sums = url_downloader->download({hash_url}).split('\n');
    QString hash;

    for (const QString line : sha256_sums) // intentional copy
    {
        if (line.trimmed().endsWith(image_file))
        {
            hash = line.split(' ').first();
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

auto full_image_info_for(const QMap<QString, CustomImageInfo>& custom_image_info, mp::URLDownloader* url_downloader,
                         const QString& path_prefix)
{
    std::vector<mp::VMImageInfo> default_images;

    for (const auto& image_info : custom_image_info.toStdMap())
    {
        auto image_file = image_info.first;
        auto prefix =
            path_prefix.isEmpty() ? image_info.second.url_prefix : QUrl::fromLocalFile(path_prefix).toString();
        QString image_url{prefix + image_info.first};
        QString hash_url{prefix + QStringLiteral("SHA256SUMS")};

        auto base_image_info = base_image_info_for(url_downloader, image_url, hash_url, image_file);
        mp::VMImageInfo full_image_info{image_info.second.aliases,
                                        image_info.second.os,
                                        image_info.second.release,
                                        image_info.second.release_string,
                                        true,      // supported
                                        image_url, // image_location
                                        image_info.second.kernel_location,
                                        image_info.second.initrd_location,
                                        base_image_info.hash, // id
                                        "",
                                        base_image_info.last_modified, // version
                                        0,
                                        true};

        default_images.push_back(full_image_info);
    }

    auto map = map_aliases_to_vm_info_for(default_images);

    return std::unique_ptr<mp::CustomManifest>(new mp::CustomManifest{std::move(default_images), std::move(map)});
}

} // namespace

mp::CustomVMImageHost::CustomVMImageHost(URLDownloader* downloader, std::chrono::seconds manifest_time_to_live)
    : CustomVMImageHost{downloader, manifest_time_to_live, ""}
{
}

mp::CustomVMImageHost::CustomVMImageHost(URLDownloader* downloader, std::chrono::seconds manifest_time_to_live,
                                         const QString& path_prefix)
    : CommonVMImageHost{manifest_time_to_live},
      url_downloader{downloader},
      path_prefix{path_prefix},
      custom_image_info{},
      remotes{no_remote, snapcraft_remote}
{
}

mp::optional<mp::VMImageInfo> mp::CustomVMImageHost::info_for(const Query& query)
{
    check_remote_is_supported(query.remote_name);

    auto custom_manifest = manifest_from(query.remote_name);

    auto it = custom_manifest->image_records.find(query.release);

    if (it == custom_manifest->image_records.end())
        return nullopt;

    return *it->second;
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_info_for(const Query& query)
{
    check_remote_is_supported(query.remote_name);

    check_alias_is_supported(query.release, query.remote_name);

    std::vector<mp::VMImageInfo> images;

    auto image = info_for(query);
    if (image != nullopt)
        images.push_back(*image);

    return images;
}

mp::VMImageInfo mp::CustomVMImageHost::info_for_full_hash_impl(const std::string& full_hash)
{
    return {};
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_images_for(const std::string& remote_name,
                                                                   const bool allow_unsupported)
{
    check_remote_is_supported(remote_name);

    std::vector<mp::VMImageInfo> images;
    auto custom_manifest = manifest_from(remote_name);

    for (const auto& product : custom_manifest->products)
    {
        if (!check_all_aliases_are_supported(product.aliases, remote_name))
        {
            continue;
        }

        images.push_back(product);
    }

    return images;
}

void mp::CustomVMImageHost::for_each_entry_do_impl(const Action& action)
{
    for (const auto& manifest : custom_image_info)
    {
        if (!mp::platform::is_remote_supported(manifest.first))
            continue;

        for (const auto& info : manifest.second->products)
        {
            if (!check_all_aliases_are_supported(info.aliases, manifest.first))
            {
                continue;
            }

            action(manifest.first, info);
        }
    }
}

std::vector<std::string> mp::CustomVMImageHost::supported_remotes()
{
    return remotes;
}

void mp::CustomVMImageHost::fetch_manifests()
{
    for (const auto& spec :
         {std::make_pair(no_remote, multipass_image_info), std::make_pair(snapcraft_remote, snapcraft_image_info)})
    {
        try
        {
            custom_image_info.emplace(spec.first, full_image_info_for(spec.second, url_downloader, path_prefix));
        }
        catch (mp::DownloadException& e)
        {
            on_manifest_update_failure(e.what());
        }
    }
}

void mp::CustomVMImageHost::clear()
{
    custom_image_info.clear();
}

mp::CustomManifest* mp::CustomVMImageHost::manifest_from(const std::string& remote_name)
{
    update_manifests();

    auto it = custom_image_info.find(remote_name);
    if (it == custom_image_info.end())
        throw std::runtime_error(fmt::format("Remote \"{}\" is unknown or unreachable.", remote_name));

    return it->second.get();
}

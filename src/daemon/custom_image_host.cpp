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

#include "custom_image_host.h"

#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/unsupported_remote_exception.h>

#include <multipass/format.h>

#include <QMap>
#include <QUrl>

#include <utility>

namespace mp = multipass;

namespace
{
constexpr auto no_remote = "";

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
    QString release_codename;
};

const QMap<QString, QMap<QString, CustomImageInfo>> multipass_image_info{
    {{"x86_64"},
     {{{"ubuntu-core-16-amd64.img.xz"},
       {"https://cdimage.ubuntu.com/ubuntu-core/16/stable/current/",
        {"core", "core16"},
        "Ubuntu",
        "core-16",
        "Core 16",
        "Core 16"}},
      {{"ubuntu-core-18-amd64.img.xz"},
       {"https://cdimage.ubuntu.com/ubuntu-core/18/stable/current/",
        {"core18"},
        "Ubuntu",
        "core-18",
        "Core 18",
        "Core 18"}},
      {{"ubuntu-core-20-amd64.img.xz"},
       {"https://cdimage.ubuntu.com/ubuntu-core/20/stable/current/",
        {"core20"},
        "Ubuntu",
        "core-20",
        "Core 20",
        "Core 20"}},
      {{"ubuntu-core-22-amd64.img.xz"},
       {"https://cdimage.ubuntu.com/ubuntu-core/22/stable/current/",
        {"core22"},
        "Ubuntu",
        "core-22",
        "Core 22",
        "Core 22"}},
      {{"ubuntu-core-24-amd64.img.xz"},
       {"https://cdimage.ubuntu.com/ubuntu-core/24/stable/current/",
        {"core24"},
        "Ubuntu",
        "core-24",
        "Core 24",
        "Core 24"}}}}};

auto base_image_info_for(mp::URLDownloader* url_downloader, const QString& image_url, const QString& hash_url,
                         const QString& image_file, const bool is_force_update_from_network = false)
{
    const auto last_modified = QLocale::c().toString(url_downloader->last_modified({image_url}), "yyyyMMdd");
    const auto sha256_sums = url_downloader->download({hash_url}, is_force_update_from_network).split('\n');
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
                         const bool is_force_update_from_network = false)
{
    auto fetch_one_image_info =
        [is_force_update_from_network,
         url_downloader](const std::pair<const QString, CustomImageInfo>& image_info_pair) -> mp::VMImageInfo {
        const auto& [image_file_name, custom_image_info] = image_info_pair;
        const QString image_url{custom_image_info.url_prefix + image_info_pair.first};
        const QString hash_url{custom_image_info.url_prefix + QStringLiteral("SHA256SUMS")};

        const auto base_image_info =
            base_image_info_for(url_downloader, image_url, hash_url, image_file_name, is_force_update_from_network);

        return mp::VMImageInfo{custom_image_info.aliases,
                               custom_image_info.os,
                               custom_image_info.release,
                               custom_image_info.release_string,
                               custom_image_info.release_codename,
                               true,                 // supported
                               image_url,            // image_location
                               base_image_info.hash, // id
                               "",
                               base_image_info.last_modified, // version
                               0,
                               true};
    };

    return std::make_unique<mp::CustomManifest>(
        mp::utils::parallel_transform(custom_image_info.toStdMap(), fetch_one_image_info));
}

} // namespace

mp::CustomManifest::CustomManifest(std::vector<VMImageInfo>&& images)
    : products{std::move(images)}, image_records{map_aliases_to_vm_info_for(products)}
{
}

mp::CustomVMImageHost::CustomVMImageHost(const QString& arch, URLDownloader* downloader)
    : arch{arch}, url_downloader{downloader}, custom_image_info{}, remotes{no_remote}
{
}

std::optional<mp::VMImageInfo> mp::CustomVMImageHost::info_for(const Query& query)
{
    check_alias_is_supported(query.release, query.remote_name);

    auto custom_manifest = manifest_from(query.remote_name);

    auto it = custom_manifest->image_records.find(query.release);

    if (it == custom_manifest->image_records.end())
        return std::nullopt;

    return *it->second;
}

std::vector<std::pair<std::string, mp::VMImageInfo>> mp::CustomVMImageHost::all_info_for(const Query& query)
{
    std::vector<std::pair<std::string, mp::VMImageInfo>> images;

    auto image = info_for(query);
    if (image != std::nullopt)
        images.push_back(std::make_pair(query.remote_name, *image));

    return images;
}

mp::VMImageInfo mp::CustomVMImageHost::info_for_full_hash_impl(const std::string& full_hash)
{
    return {};
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_images_for(const std::string& remote_name,
                                                                   const bool allow_unsupported)
{
    std::vector<mp::VMImageInfo> images;
    auto custom_manifest = manifest_from(remote_name);

    auto pred = [this, &remote_name](const auto& product) {
        return alias_verifies_image_is_supported(product.aliases, remote_name);
    };

    std::copy_if(custom_manifest->products.begin(), custom_manifest->products.end(), std::back_inserter(images), pred);

    return images;
}

void mp::CustomVMImageHost::for_each_entry_do_impl(const Action& action)
{
    for (const auto& manifest : custom_image_info)
    {
        for (const auto& info : manifest.second->products)
        {
            if (alias_verifies_image_is_supported(info.aliases, manifest.first))
                action(manifest.first, info);
        }
    }
}

std::vector<std::string> mp::CustomVMImageHost::supported_remotes()
{
    return remotes;
}

void mp::CustomVMImageHost::fetch_manifests(const bool is_force_update_from_network)
{
    for (const auto& spec : {std::make_pair(no_remote, multipass_image_info[arch])})
    {
        try
        {
            check_remote_is_supported(spec.first);
            std::unique_ptr<mp::CustomManifest> custom_manifest =
                full_image_info_for(spec.second, url_downloader, is_force_update_from_network);
            custom_image_info.emplace(spec.first, std::move(custom_manifest));
        }
        catch (mp::DownloadException& e)
        {
            throw e;
        }
        catch (const mp::UnsupportedRemoteException&)
        {
            continue;
        }
    }
}

void mp::CustomVMImageHost::clear()
{
    custom_image_info.clear();
}

mp::CustomManifest* mp::CustomVMImageHost::manifest_from(const std::string& remote_name)
{
    check_remote_is_supported(remote_name);

    auto it = custom_image_info.find(remote_name);
    if (it == custom_image_info.end())
        throw std::runtime_error(fmt::format("Remote \"{}\" is unknown or unreachable.", remote_name));

    return it->second.get();
}

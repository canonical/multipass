/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "ubuntu_image_host.h"

#include <multipass/query.h>
#include <multipass/simple_streams_index.h>
#include <multipass/url_downloader.h>

#include <fmt/format.h>

#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <unordered_set>

namespace mp = multipass;

namespace
{
constexpr auto index_path = "streams/v1/index.json";

auto download_manifest(const QString& host_url, mp::URLDownloader* url_downloader)
{
    auto json_index = url_downloader->download({host_url + index_path});
    auto index = mp::SimpleStreamsIndex::fromJson(json_index);

    auto json_manifest = url_downloader->download({host_url + index.manifest_path});
    return mp::SimpleStreamsManifest::fromJson(json_manifest);
}

mp::VMImageInfo with_location_fully_resolved(const QString& host_url, const mp::VMImageInfo& info)
{
    return {info.aliases,
            info.release,
            info.release_title,
            info.supported,
            host_url + info.image_location,
            host_url + info.kernel_location,
            host_url + info.initrd_location,
            info.id,
            info.version,
            info.size};
}

auto key_from(const std::string& search_string)
{
    auto key = QString::fromStdString(search_string);
    if (key.isEmpty())
        key = "default";
    return key;
}

mp::VMImageInfo ubuntu_core_info(mp::URLDownloader* url_downloader)
{
    QString url{"http://cdimage.ubuntu.com/ubuntu-core/16/current/ubuntu-core-16-amd64.img.xz"};
    auto sha256_sums =
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

    auto last_modified = url_downloader->last_modified({url});

    return {{"core"}, "core-16", "Core 16", true, url, "", "", hash, last_modified.toString("yyyyMMdd"), 0};
}
}

mp::UbuntuVMImageHost::UbuntuVMImageHost(std::vector<std::pair<std::string, std::string>> remotes,
                                         URLDownloader* downloader, std::chrono::seconds manifest_time_to_live)
    : manifest_time_to_live{manifest_time_to_live}, url_downloader{downloader}, remotes{remotes}
{
    QTimer::singleShot(0, [this]() { update_manifest(); });
}

mp::VMImageInfo mp::UbuntuVMImageHost::info_for(const Query& query)
{
    auto key = key_from(query.release);
    if (key == "core")
    {
        return ubuntu_core_info(url_downloader);
    }

    const auto remote_name = query.remote_name.empty() ? release_remote : query.remote_name;
    auto& manifest = manifest_from(remote_name);

    const VMImageInfo* info{nullptr};

    match_alias(key, &info, *manifest);

    if (!info)
    {
        int num_matches = 0;

        auto predicate = [&](const VMImageInfo& entry) {
            auto partial_match = false;
            if (entry.id.startsWith(key))
            {
                info = &entry;
                partial_match = true;
            }
            return partial_match;
        };
        num_matches += std::count_if(manifest->products.begin(), manifest->products.end(), predicate);
        if (num_matches > 1)
            throw std::runtime_error(fmt::format("Too many images matching \"{}\"", query.release));
    }

    if (info)
    {
        if (!info->supported)
            throw std::runtime_error(fmt::format("The {} release is no longer supported.", query.release));

        return with_location_fully_resolved(QString::fromStdString(remote_url_from(remote_name)), *info);
    }

    throw std::runtime_error(fmt::format("Unable to find an image matching \"{}\"", query.release));
}

std::vector<mp::VMImageInfo> mp::UbuntuVMImageHost::all_info_for(const Query& query)
{
    std::vector<mp::VMImageInfo> images;

    auto key = key_from(query.release);
    if (key == "core")
    {
        images.push_back(ubuntu_core_info(url_downloader));
        return images;
    }

    const auto remote_name = query.remote_name.empty() ? release_remote : query.remote_name;
    auto& manifest = manifest_from(remote_name);

    const VMImageInfo* info{nullptr};

    match_alias(key, &info, *manifest);

    if (info)
    {
        if (!info->supported)
            throw std::runtime_error(fmt::format("The {} release is no longer supported.", query.release));

        images.push_back(*info);
    }
    else
    {
        std::unordered_set<std::string> found_hashes;

        for (const auto& entry : manifest->products)
        {
            if (entry.id.startsWith(key) && entry.supported &&
                found_hashes.find(entry.id.toStdString()) == found_hashes.end())
            {
                images.push_back(
                    with_location_fully_resolved(QString::fromStdString(remote_url_from(remote_name)), entry));
                found_hashes.insert(entry.id.toStdString());
            }
        }
    }

    if (images.empty())
        throw std::runtime_error(fmt::format("Unable to find an image matching \"{}\"", query.release));

    return images;
}

mp::VMImageInfo mp::UbuntuVMImageHost::info_for_full_hash(const std::string& full_hash)
{
    update_manifest();

    for (const auto& manifest : manifests)
    {
        for (const auto& product : manifest.second->products)
        {
            if (product.id.toStdString() == full_hash)
            {
                return with_location_fully_resolved(QString::fromStdString(remote_url_from(manifest.first)), product);
            }
        }
    }

    throw std::runtime_error(fmt::format("Unable to find an image matching hash \"{}\"", full_hash));

    return mp::VMImageInfo{{}, {}, {}, {}, {}, {}, {}, {}, {}, -1};
}

std::vector<mp::VMImageInfo> mp::UbuntuVMImageHost::all_images_for(const std::string& remote_name)
{
    std::vector<mp::VMImageInfo> images;
    auto& manifest = manifest_from(remote_name);

    for (const auto& entry : manifest->products)
    {
        if (entry.supported)
        {
            images.push_back(with_location_fully_resolved(QString::fromStdString(remote_url_from(remote_name)), entry));
        }
    }

    if (images.empty())
        throw std::runtime_error(fmt::format("Unable to find images for remote \"{}\"", remote_name));

    return images;
}

void mp::UbuntuVMImageHost::for_each_entry_do(const Action& action)
{
    update_manifest();

    for (const auto& manifest : manifests)
    {
        for (const auto& product : manifest.second->products)
        {
            action(manifest.first,
                   with_location_fully_resolved(QString::fromStdString(remote_url_from(manifest.first)), product));
        }
    }

    action("release", ubuntu_core_info(url_downloader));
}

std::string mp::UbuntuVMImageHost::get_default_remote()
{
    return release_remote;
}

void mp::UbuntuVMImageHost::update_manifest()
{
    const auto now = std::chrono::steady_clock::now();
    if ((now - last_update) > manifest_time_to_live || manifests.empty())
    {
        manifests.clear();

        for (const auto& remote : remotes)
        {
            manifests.emplace_back(
                std::make_pair(remote.first, download_manifest(QString::fromStdString(remote.second), url_downloader)));
        }
        last_update = now;
    }
}

std::unique_ptr<mp::SimpleStreamsManifest>& mp::UbuntuVMImageHost::manifest_from(const std::string& remote)
{
    update_manifest();

    auto it = std::find_if(manifests.begin(), manifests.end(),
                           [&remote](const std::pair<std::string, std::unique_ptr<SimpleStreamsManifest>>& element) {
                               return element.first == remote;
                           });

    if (it == manifests.cend())
        throw std::runtime_error(fmt::format("Remote \"{}\" is unknown.", remote));

    return it->second;
}

void mp::UbuntuVMImageHost::match_alias(const QString& key, const VMImageInfo** info,
                                        const mp::SimpleStreamsManifest& manifest)
{
    auto it = manifest.image_records.find(key);
    if (it != manifest.image_records.end())
    {
        *info = it.value();
    }
}

std::string mp::UbuntuVMImageHost::remote_url_from(const std::string& remote_name)
{
    auto it = std::find_if(
        remotes.cbegin(), remotes.cend(),
        [&remote_name](const std::pair<std::string, std::string>& element) { return element.first == remote_name; });

    if (it == remotes.cend())
        throw std::runtime_error(fmt::format("Cannot find remote \"{}\".", remote_name));

    return it->second;
}

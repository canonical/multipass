/*
 * Copyright (C) 2017 Canonical, Ltd.
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
}

mp::UbuntuVMImageHost::UbuntuVMImageHost(QStringList host_urls, URLDownloader* downloader,
                                         std::chrono::seconds manifest_time_to_live)
    : manifest_time_to_live{manifest_time_to_live}, url_downloader{downloader}, host_urls{host_urls}
{
}

mp::VMImageInfo mp::UbuntuVMImageHost::info_for(const Query& query)
{
    auto key = key_from(query.release);

    const VMImageInfo* info{nullptr};
    std::string host_url;

    match_alias(key, &info, host_url);

    if (!info)
    {
        int num_matches = 0;
        std::unordered_set<std::string> found_hashes;

        for (const auto& manifest : manifests)
        {
            auto predicate = [&](const VMImageInfo& entry) {
                auto partial_match = false;
                if (entry.id.startsWith(key) && found_hashes.find(entry.id.toStdString()) == found_hashes.end())
                {
                    info = &entry;
                    host_url = manifest.first;
                    partial_match = true;
                    found_hashes.insert(entry.id.toStdString());
                }
                return partial_match;
            };
            num_matches += std::count_if(manifest.second->products.begin(), manifest.second->products.end(), predicate);
            if (num_matches > 1)
                throw std::runtime_error("Too many images matching \"" + query.release + "\"");
        }
    }

    if (info)
    {
        if (!info->supported)
            throw std::runtime_error("The " + query.release + " release is no longer supported.");

        return with_location_fully_resolved(QString::fromStdString(host_url), *info);
    }
    else
        throw std::runtime_error("Unable to find an image matching \"" + query.release + "\"");

    // Should not reach this
    return *info;
}

std::vector<mp::VMImageInfo> mp::UbuntuVMImageHost::all_info_for(const Query& query)
{
    std::vector<mp::VMImageInfo> images;

    auto key = key_from(query.release);

    const VMImageInfo* info{nullptr};
    std::string host_url;

    match_alias(key, &info, host_url);

    if (info)
    {
        if (!info->supported)
            throw std::runtime_error("The " + query.release + " release is no longer supported.");

        images.push_back(*info);
    }
    else
    {
        std::unordered_set<std::string> found_hashes;

        for (const auto& manifest : manifests)
        {
            for (const auto& entry : manifest.second->products)
            {
                if (entry.id.startsWith(key) && entry.supported &&
                    found_hashes.find(entry.id.toStdString()) == found_hashes.end())
                {
                    images.push_back(with_location_fully_resolved(QString::fromStdString(manifest.first), entry));
                    found_hashes.insert(entry.id.toStdString());
                }
            }
        }
    }

    if (images.empty())
        throw std::runtime_error("Unable to find an image matching \"" + query.release + "\"");
    return images;
}

void mp::UbuntuVMImageHost::for_each_entry_do(const Action& action)
{
    update_manifest();

    for (const auto& manifest : manifests)
    {
        for (const auto& product : manifest.second->products)
        {
            action(with_location_fully_resolved(QString::fromStdString(manifest.first), product));
        }
    }
}

void mp::UbuntuVMImageHost::update_manifest()
{
    const auto now = std::chrono::steady_clock::now();
    if ((now - last_update) > manifest_time_to_live || manifests.empty())
    {
        manifests.clear();

        for (const auto& host_url : host_urls)
        {
            manifests.emplace_back(host_url.toStdString(), download_manifest(host_url, url_downloader));
        }
        last_update = now;
    }
}

void mp::UbuntuVMImageHost::match_alias(const QString& key, const VMImageInfo** info, std::string& host_url)
{
    update_manifest();

    // Check all manifests for alias match first
    for (const auto& manifest : manifests)
    {
        auto it = manifest.second->image_records.find(key);
        if (it != manifest.second->image_records.end())
        {
            *info = it.value();
            host_url = manifest.first;
            break;
        }
    }
}

/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include "ubuntu_image_host.h"

#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/simple_streams_index.h>
#include <multipass/url_downloader.h>

#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/manifest_exceptions.h>
#include <multipass/exceptions/unsupported_image_exception.h>

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
    return mp::SimpleStreamsManifest::fromJson(json_manifest, host_url);
}

mp::VMImageInfo with_location_fully_resolved(const QString& host_url, const mp::VMImageInfo& info)
{
    return {info.aliases,
            info.os,
            info.release,
            info.release_title,
            info.supported,
            host_url + info.image_location,
            host_url + info.kernel_location,
            host_url + info.initrd_location,
            info.id,
            info.stream_location,
            info.version,
            info.size,
            info.verify};
}

auto key_from(const std::string& search_string)
{
    auto key = QString::fromStdString(search_string);
    if (key.isEmpty())
        key = "default";
    return key;
}
} // namespace

mp::UbuntuVMImageHost::UbuntuVMImageHost(std::vector<std::pair<std::string, std::string>> remotes,
                                         URLDownloader* downloader, std::chrono::seconds manifest_time_to_live)
    : CommonVMImageHost{manifest_time_to_live}, url_downloader{downloader}, remotes{std::move(remotes)}
{
}

mp::optional<mp::VMImageInfo> mp::UbuntuVMImageHost::info_for(const Query& query)
{
    std::vector<std::string> remotes;

    if (!query.remote_name.empty())
    {
        check_remote_is_supported(query.remote_name);

        remotes.push_back(query.remote_name);
    }
    else
    {
        for (const auto& remote_name : std::vector<std::string>{release_remote, daily_remote})
        {
            if (mp::platform::is_remote_supported(remote_name))
            {
                remotes.push_back(remote_name);
            }
        }
    }

    auto key = key_from(query.release);
    mp::SimpleStreamsManifest* manifest;

    for (const auto& remote_name : remotes)
    {
        const VMImageInfo* info{nullptr};

        manifest = manifest_from(remote_name);
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
            if (!info->supported && !query.allow_unsupported)
                throw mp::UnsupportedImageException(query.release);

            return with_location_fully_resolved(QString::fromStdString(remote_url_from(remote_name)), *info);
        }
    }

    return nullopt;
}

std::vector<mp::VMImageInfo> mp::UbuntuVMImageHost::all_info_for(const Query& query)
{
    std::string remote_name;

    if (!query.remote_name.empty())
    {
        check_remote_is_supported(query.remote_name);

        remote_name = query.remote_name;
    }
    else
    {
        remote_name = release_remote;
    }

    check_alias_is_supported(query.release, query.remote_name);

    std::vector<mp::VMImageInfo> images;

    auto key = key_from(query.release);
    mp::SimpleStreamsManifest* manifest;
    const VMImageInfo* info{nullptr};

    manifest = manifest_from(remote_name);
    match_alias(key, &info, *manifest);

    if (info)
    {
        if (!info->supported && !query.allow_unsupported)
            throw mp::UnsupportedImageException(query.release);

        images.push_back(*info);
    }
    else
    {
        std::unordered_set<std::string> found_hashes;

        for (const auto& entry : manifest->products)
        {
            if (entry.id.startsWith(key) && (entry.supported || query.allow_unsupported) &&
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

mp::VMImageInfo mp::UbuntuVMImageHost::info_for_full_hash_impl(const std::string& full_hash)
{
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

    // TODO: Throw a specific exception type here so callers can be more specific about what to catch
    //       and what to allow through.
    throw std::runtime_error(fmt::format("Unable to find an image matching hash \"{}\"", full_hash));
}

std::vector<mp::VMImageInfo> mp::UbuntuVMImageHost::all_images_for(const std::string& remote_name,
                                                                   const bool allow_unsupported)
{
    check_remote_is_supported(remote_name);

    std::vector<mp::VMImageInfo> images;
    auto manifest = manifest_from(remote_name);

    for (const auto& entry : manifest->products)
    {
        if (!check_all_aliases_are_supported(entry.aliases, remote_name))
        {
            continue;
        }

        if (entry.supported || allow_unsupported)
        {
            images.push_back(with_location_fully_resolved(QString::fromStdString(remote_url_from(remote_name)), entry));
        }
    }

    if (images.empty())
        throw std::runtime_error(fmt::format("Unable to find images for remote \"{}\"", remote_name));

    return images;
}

void mp::UbuntuVMImageHost::for_each_entry_do_impl(const Action& action)
{
    for (const auto& manifest : manifests)
    {
        if (!mp::platform::is_remote_supported(manifest.first))
            continue;

        for (const auto& product : manifest.second->products)
        {
            if (!check_all_aliases_are_supported(product.aliases, manifest.first))
            {
                continue;
            }

            action(manifest.first,
                   with_location_fully_resolved(QString::fromStdString(remote_url_from(manifest.first)), product));
        }
    }
}

std::vector<std::string> mp::UbuntuVMImageHost::supported_remotes()
{
    std::vector<std::string> supported_remotes;

    for (const auto& remote : remotes)
    {
        supported_remotes.push_back(remote.first);
    }

    return supported_remotes;
}

void mp::UbuntuVMImageHost::fetch_manifests()
{
    for (const auto& remote : remotes)
    {
        try
        {
            manifests.emplace_back(
                std::make_pair(remote.first, download_manifest(QString::fromStdString(remote.second), url_downloader)));
        }
        catch (mp::EmptyManifestException& /* e */)
        {
            on_manifest_empty(fmt::format("Did not find any supported products in \"{}\"", remote.first));
        }
        catch (mp::GenericManifestException& e)
        {
            on_manifest_update_failure(e.what());
        }
        catch (mp::DownloadException& e)
        {
            on_manifest_update_failure(e.what());
        }
    }
}

void mp::UbuntuVMImageHost::clear()
{
    manifests.clear();
}

mp::SimpleStreamsManifest* mp::UbuntuVMImageHost::manifest_from(const std::string& remote)
{
    update_manifests();

    auto it = std::find_if(manifests.begin(), manifests.end(),
                           [&remote](const std::pair<std::string, std::unique_ptr<SimpleStreamsManifest>>& element) {
                               return element.first == remote;
                           });

    if (it == manifests.cend())
        throw std::runtime_error(fmt::format("Remote \"{}\" is unknown or unreachable.", remote));

    return it->second.get();
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
    std::string url;

    auto it = std::find_if(
        remotes.cbegin(), remotes.cend(),
        [&remote_name](const std::pair<std::string, std::string>& element) { return element.first == remote_name; });

    if (it != remotes.cend())
        url = it->second;

    return url;
}

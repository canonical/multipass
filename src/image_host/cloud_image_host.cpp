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

#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/image_not_found_exception.h>
#include <multipass/exceptions/unsupported_image_exception.h>
#include <multipass/image_host/cloud_image_host.h>
#include <multipass/logging/log.h>
#include <multipass/query.h>
#include <multipass/utils.h>

#include <unordered_set>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "CloudImageHost";

mp::VMImageInfo with_location_fully_resolved(const QString& host_url, const mp::VMImageInfo& info)
{
    return {info.aliases,
            info.os,
            info.release,
            info.release_title,
            info.release_codename,
            info.supported,
            host_url + info.image_location,
            info.id,
            info.stream_location,
            info.version,
            info.size,
            info.verify};
}
} // namespace

mp::CloudImageHost::CloudImageHost(
    std::unordered_map<std::string, std::unique_ptr<CloudImageRemote>> remotes,
    URLDownloader* downloader)
    : remotes(std::move(remotes)), url_downloader(downloader)
{
}

void mp::CloudImageHost::for_each_remote_do(const Action& action)
{
    for (const auto& [remote_label, remote] : remotes)
    {
        for (const auto& product : remote->get_products())
        {
            if (remote->admits_image(product))
            {
                action(remote_label,
                       with_location_fully_resolved(QString::fromStdString(remote->get_url()),
                                                    product));
            }
        }
    }
}

mp::VMImageInfo mp::CloudImageHost::info_for_full_hash(const std::string& full_hash) const
{
    for (const auto& [_, remote] : remotes)
    {
        for (const auto& product : remote->get_products())
        {
            if (product.id.toStdString() == full_hash)
            {
                return with_location_fully_resolved(QString::fromStdString(remote->get_url()),
                                                    product);
            }
        }
    }

    throw mp::ImageNotFoundException(
        fmt::format("Unable to find an image matching hash \"{}\"", full_hash));
}

std::vector<mp::VMImageInfo> mp::CloudImageHost::all_images_for(const std::string& remote_name,
                                                                const bool allow_unsupported) const
{
    std::vector<mp::VMImageInfo> images;
    auto& remote = remotes.at(remote_name);

    for (const auto& product : remote->get_products())
    {
        if ((product.supported || allow_unsupported) && remote->admits_image(product))
        {
            images.push_back(
                with_location_fully_resolved(QString::fromStdString(remote->get_url()), product));
        }
    }

    if (images.empty())
        throw std::runtime_error(
            fmt::format("Unable to find images for remote \"{}\"", remote_name));

    return images;
}

std::vector<std::pair<std::string, mp::VMImageInfo>> mp::CloudImageHost::all_info_for(
    const Query& query) const
{
    auto key = query.release.empty() ? "default" : query.release;
    std::vector<std::pair<std::string, mp::VMImageInfo>> images;

    for (const auto& [remote_label, remote] : remotes)
    {
        if (const auto* image = remote->match_hash(key); image)
        {
            if (!remote->admits_image(*image))
                continue;

            if (!image->supported && !query.allow_unsupported)
                throw mp::UnsupportedImageException(query.release);

            images.push_back(std::make_pair(
                remote_label,
                with_location_fully_resolved(QString::fromStdString(remote->get_url()), *image)));
        }
        else
        {
            std::unordered_set<std::string> found_hashes;

            for (const auto& product : remote->get_products())
            {
                if (product.id.startsWith(QString::fromStdString(key)) &&
                    remote->admits_image(product) &&
                    (product.supported || query.allow_unsupported) &&
                    found_hashes.find(product.id.toStdString()) == found_hashes.end())
                {
                    images.push_back(std::make_pair(
                        remote_label,
                        with_location_fully_resolved(QString::fromStdString(remote->get_url()),
                                                     product)));
                    found_hashes.insert(product.id.toStdString());
                }
            }
        }
    }

    return images;
}

std::optional<mp::VMImageInfo> mp::CloudImageHost::info_for(const Query& query) const
{
    auto images = all_info_for(query);

    if (images.size() == 0)
        return std::nullopt;

    auto key = query.release.empty() ? "default" : query.release;
    auto image_hash = images.front().second.id;

    // If a partial hash query matches more than once, throw an exception
    if (images.size() > 1 && key != image_hash.toStdString() &&
        image_hash.startsWith(QString::fromStdString(key)))
        throw std::runtime_error(fmt::format("Too many images matching \"{}\"", query.release));

    // It's not a hash match, so choose the first one no matter what
    return images.front().second;
}

void mp::CloudImageHost::fetch_remotes(const bool force_update)
{
    auto fetch_remote =
        [this, force_update](
            const std::pair<const std::string, std::unique_ptr<CloudImageRemote>>& remote_pair)
        -> void {
        const auto& [remote_name, remote] = remote_pair;

        mpl::log(mpl::Level::info,
                 category,
                 fmt::format("Updating remote {}; Force: {}", remote_name, force_update));

        remote->update_products(url_downloader, force_update);
    };

    mp::utils::parallel_for_each(remotes, fetch_remote);
}

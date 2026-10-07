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

#include <multipass/constants.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/manifest_exceptions.h>
#include <multipass/image_host/ubuntu_image_host.h>
#include <multipass/platform.h>
#include <multipass/settings/settings.h>
#include <multipass/simple_streams_index.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include <QUrl>

#include <algorithm>

namespace mp = multipass;

namespace
{
constexpr auto index_path = "streams/v1/index.json";

auto download_manifest(const std::string& host_url,
                       mp::URLDownloader* url_downloader,
                       bool force_update)
{
    auto index_url = QString::fromStdString(host_url + index_path);
    auto json_index = url_downloader->download(index_url, force_update);
    auto index = mp::SimpleStreamsIndex::get_image_downloads(std::string_view{json_index});

    auto manifest_url = QString::fromStdString(host_url + index.manifest_path);
    return url_downloader->download(manifest_url, force_update);
}

} // namespace

mp::UbuntuVMImageRemote::UbuntuVMImageRemote(std::string official_host,
                                             std::string uri,
                                             std::optional<std::string> mirror_key)
    : UbuntuVMImageRemote(std::move(official_host),
                          std::move(uri),
                          &default_image_mutator,
                          std::move(mirror_key))
{
}

mp::UbuntuVMImageRemote::UbuntuVMImageRemote(std::string official_host,
                                             std::string uri,
                                             std::function<bool(VMImageInfo&)> custom_image_mutator,
                                             std::optional<std::string> mirror_key)
    : official_host(std::move(official_host)),
      uri(std::move(uri)),
      image_mutator{custom_image_mutator},
      mirror_key(std::move(mirror_key))
{
}

const std::string mp::UbuntuVMImageRemote::get_official_url() const
{
    return official_host + uri;
}

const std::optional<std::string> mp::UbuntuVMImageRemote::get_mirror_url() const
{
    if (mirror_key)
    {
        if (auto mirror = MP_SETTINGS.get(QString::fromStdString(mirror_key.value()));
            !mirror.isEmpty())
        {
            return std::make_optional(mirror.toStdString() + uri);
        }
    }

    return std::nullopt;
}

bool mp::UbuntuVMImageRemote::apply_image_mutator(VMImageInfo& info) const
{
    return image_mutator(info);
}

bool mp::UbuntuVMImageRemote::default_image_mutator(VMImageInfo&)
{
    return true;
}

mp::UbuntuVMImageHost::UbuntuVMImageHost(
    std::vector<std::pair<std::string, UbuntuVMImageRemote>> remotes,
    URLDownloader* downloader)
    : BaseVMImageHost{downloader}, remotes{std::move(remotes)}
{
}

std::vector<std::string> mp::UbuntuVMImageHost::supported_remotes() const
{
    std::vector<std::string> supported_remotes;

    for (const auto& [remote_name, _] : remotes)
    {
        supported_remotes.push_back(remote_name);
    }

    return supported_remotes;
}

const std::vector<mp::VMImageInfo>* mp::UbuntuVMImageHost::images_for_remote(
    const std::string& remote) const
{
    auto it = all_images_by_remote.find(remote);
    return it != all_images_by_remote.end() ? &it->second : nullptr;
}

void mp::UbuntuVMImageHost::fetch_manifests(bool force_update)
{
    auto fetch_one_remote =
        [this, force_update](const std::pair<std::string, UbuntuVMImageRemote>& remote_pair)
        -> std::pair<std::string, std::unique_ptr<SimpleStreamsManifest>> {
        const auto& [remote_name, remote_info] = remote_pair;

        try
        {
            auto official_site = remote_info.get_official_url();
            auto manifest_bytes_from_official = download_manifest(official_site,
                                                                  url_downloader,
                                                                  force_update);

            auto mirror_site = remote_info.get_mirror_url();
            std::optional<QByteArray> manifest_bytes_from_mirror = std::nullopt;
            if (mirror_site)
            {
                auto bytes = download_manifest(mirror_site.value(), url_downloader, force_update);
                manifest_bytes_from_mirror = std::make_optional(bytes);
            }

            auto manifest = mp::SimpleStreamsManifest::fromJson(
                manifest_bytes_from_official,
                manifest_bytes_from_mirror,
                QString::fromStdString(mirror_site.value_or(official_site)),
                [&remote_info](VMImageInfo& info) {
                    return remote_info.apply_image_mutator(info);
                });

            return std::make_pair(remote_name, std::move(manifest));
        }
        catch (mp::EmptyManifestException& /* e */)
        {
            on_manifest_empty(
                fmt::format("Did not find any supported products in \"{}\"", remote_name));
        }
        catch (mp::GenericManifestException& e)
        {
            on_manifest_update_failure(e.what());
        }
        catch (mp::DownloadException& e)
        {
            throw e;
        }
        return {};
    };

    auto local_manifests = mp::utils::parallel_transform(remotes, fetch_one_remote);

    for (auto& [remote_name, manifest_ptr] : local_manifests)
    {
        if (manifest_ptr)
        {
            auto& cache = all_images_by_remote[remote_name];
            cache.insert(cache.end(), manifest_ptr->products.begin(), manifest_ptr->products.end());
        }
    }

    // append local_manifests to manifests
    manifests.insert(manifests.end(),
                     std::make_move_iterator(local_manifests.begin()),
                     std::make_move_iterator(local_manifests.end()));
}

void mp::UbuntuVMImageHost::clear()
{
    manifests.clear();
    all_images_by_remote.clear();
}

const mp::SimpleStreamsManifest& mp::UbuntuVMImageHost::manifest_from(
    const std::string& remote) const
{
    const auto it = std::find_if(
        manifests.cbegin(),
        manifests.cend(),
        [&remote](const std::pair<std::string, std::unique_ptr<SimpleStreamsManifest>>& element) {
            return element.first == remote;
        });

    if (it == manifests.cend())
        throw std::runtime_error(fmt::format("Remote \"{}\" is unknown or unreachable. If image "
                                             "mirror is enabled, please confirm it is valid.",
                                             remote));

    return *it->second;
}

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

#ifndef MULTIPASS_UBUNTU_IMAGE_HOST_H
#define MULTIPASS_UBUNTU_IMAGE_HOST_H

#include "common_image_host.h"

#include <multipass/constants.h>
#include <multipass/simple_streams_manifest.h>

#include <QString>

#include <string>
#include <utility>
#include <vector>

namespace multipass
{
constexpr auto release_remote = "release";
constexpr auto daily_remote = "daily";
constexpr auto appliance_remote = "appliance";

class URLDownloader;
class UbuntuVMImageRemote;
class UbuntuVMImageHost final : public CommonVMImageHost
{
public:
    UbuntuVMImageHost(std::vector<std::pair<std::string, UbuntuVMImageRemote>> remotes, URLDownloader* downloader);

    std::optional<VMImageInfo> info_for(const Query& query) override;
    std::vector<std::pair<std::string, VMImageInfo>> all_info_for(const Query& query) override;
    std::vector<VMImageInfo> all_images_for(const std::string& remote_name, const bool allow_unsupported) override;
    std::vector<std::string> supported_remotes() override;

private:
    void for_each_entry_do_impl(const Action& action) override;
    VMImageInfo info_for_full_hash_impl(const std::string& full_hash) override;
    void fetch_manifests(const bool is_force_update_from_network) override;
    void clear() override;
    SimpleStreamsManifest* manifest_from(const std::string& remote);
    const VMImageInfo* match_alias(const QString& key, const SimpleStreamsManifest& manifest) const;

    std::vector<std::pair<std::string, std::unique_ptr<SimpleStreamsManifest>>> manifests;
    URLDownloader* const url_downloader;
    std::vector<std::pair<std::string, UbuntuVMImageRemote>> remotes;
    std::string remote_url_from(const std::string& remote_name);
    QString index_path;
};
class UbuntuVMImageRemote
{
public:
    UbuntuVMImageRemote(std::string official_host, std::string uri, std::optional<QString> mirror_key = std::nullopt);
    const QString get_url() const;
    const QString get_official_url() const;
    const std::optional<QString> get_mirror_url() const;

private:
    const std::string official_host;
    const std::string uri;
    const std::optional<QString> mirror_key;
};
} // namespace multipass
#endif // MULTIPASS_UBUNTU_IMAGE_HOST_H

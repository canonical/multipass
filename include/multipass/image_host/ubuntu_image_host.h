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

#pragma once

#include <multipass/constants.h>
#include <multipass/image_host/base_image_host.h>
#include <multipass/simple_streams_manifest.h>

#include <QString>

#include <string>
#include <utility>
#include <vector>

namespace multipass
{
class URLDownloader;
class UbuntuVMImageRemote;

class UbuntuVMImageHost final : public BaseVMImageHost
{
public:
    UbuntuVMImageHost(std::vector<std::pair<std::string, UbuntuVMImageRemote>> remotes,
                      URLDownloader* downloader);

    std::vector<std::string> supported_remotes() const override;

private:
    std::optional<VMImageInfo> info_for_impl(const Query& query) const override;
    std::vector<std::pair<std::string, VMImageInfo>> all_info_for_impl(
        const Query& query) const override;
    std::vector<VMImageInfo> all_images_for_impl(const std::string& remote_name,
                                                 const bool allow_unsupported) const override;
    void for_each_entry_do_impl(const Action& action) const override;
    VMImageInfo info_for_full_hash_impl(const std::string& full_hash) const override;
    void fetch_manifests(const bool force_update) override;
    void clear() override;
    const SimpleStreamsManifest& manifest_from(const std::string& remote) const;
    const VMImageInfo* match_alias(const QString& key, const SimpleStreamsManifest& manifest) const;

    std::vector<std::pair<std::string, std::unique_ptr<SimpleStreamsManifest>>> manifests;
    std::vector<std::pair<std::string, UbuntuVMImageRemote>> remotes;
    QString index_path;
};

class UbuntuVMImageRemote
{
public:
    UbuntuVMImageRemote(std::string official_host,
                        std::string uri,
                        std::optional<QString> mirror_key = std::nullopt);
    UbuntuVMImageRemote(std::string official_host,
                        std::string uri,
                        std::function<bool(VMImageInfo&)> custom_image_mutator,
                        std::optional<QString> mirror_key = std::nullopt);
    const QString get_official_url() const;
    const std::optional<QString> get_mirror_url() const;
    bool apply_image_mutator(VMImageInfo& info) const;

private:
    static bool default_image_mutator(VMImageInfo&);

    const std::string official_host;
    const std::string uri;
    const std::function<bool(VMImageInfo&)> image_mutator;
    const std::optional<QString> mirror_key;
};
} // namespace multipass

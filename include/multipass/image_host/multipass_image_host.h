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

#include <multipass/image_host/base_image_host.h>
#include <multipass/vm_image_info.h>

#include <QString>

#include <string>
#include <vector>

namespace multipass
{
class URLDownloader;

struct MultipassManifest
{
    const std::vector<VMImageInfo> products;

    MultipassManifest(std::vector<VMImageInfo>&& images);
};

class MultipassVMImageHost final : public BaseVMImageHost
{
public:
    MultipassVMImageHost(URLDownloader* downloader);

    std::optional<VMImageInfo> info_for(const Query& query) override;
    std::vector<std::pair<std::string, VMImageInfo>> all_info_for(const Query& query) override;
    std::vector<VMImageInfo> all_images_for(const std::string& remote_name,
                                            const bool allow_unsupported) override;
    std::vector<std::string> supported_remotes() override;

private:
    void for_each_entry_do_impl(const Action& action) override;
    VMImageInfo info_for_full_hash_impl(const std::string& full_hash) override;
    void fetch_manifests(const bool force_update) override;
    void clear() override;
    MultipassManifest* manifest_from(const std::string& remote_name);

    std::pair<std::string, std::unique_ptr<MultipassManifest>> manifest;
    std::string remote;
    const QString arch;
};
} // namespace multipass

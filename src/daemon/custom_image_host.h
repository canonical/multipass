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

#ifndef MULTIPASS_CUSTOM_IMAGE_HOST
#define MULTIPASS_CUSTOM_IMAGE_HOST

#include "common_image_host.h"

#include <QString>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace multipass
{
class URLDownloader;

struct CustomManifest
{
    const std::vector<VMImageInfo> products;
    const std::unordered_map<std::string, const VMImageInfo*> image_records;

    CustomManifest(std::vector<VMImageInfo>&& images);
};

class CustomVMImageHost final : public CommonVMImageHost
{
public:
    CustomVMImageHost(const QString& arch, URLDownloader* downloader);

    std::optional<VMImageInfo> info_for(const Query& query) override;
    std::vector<std::pair<std::string, VMImageInfo>> all_info_for(const Query& query) override;
    std::vector<VMImageInfo> all_images_for(const std::string& remote_name, const bool allow_unsupported) override;
    std::vector<std::string> supported_remotes() override;

private:
    void for_each_entry_do_impl(const Action& action) override;
    VMImageInfo info_for_full_hash_impl(const std::string& full_hash) override;
    void fetch_manifests(const bool is_force_update_from_network) override;
    void clear() override;
    CustomManifest* manifest_from(const std::string& remote_name);

    const QString arch;
    URLDownloader* const url_downloader;
    std::unordered_map<std::string, std::unique_ptr<CustomManifest>> custom_image_info;
    std::vector<std::string> remotes;
};
} // namespace multipass
#endif // MULTIPASS_CUSTOM_IMAGE_HOST

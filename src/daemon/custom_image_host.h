/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/vm_image_host.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace multipass
{
class URLDownloader;

struct CustomManifest
{
    const std::vector<VMImageInfo> products;
    const std::unordered_map<std::string, const VMImageInfo*> image_records;
};

class CustomVMImageHost final : public VMImageHost
{
public:
    CustomVMImageHost(URLDownloader* downloader);
    optional<VMImageInfo> info_for(const Query& query) override;
    std::vector<VMImageInfo> all_info_for(const Query& query) override;
    VMImageInfo info_for_full_hash(const std::string& full_hash) override;
    std::vector<VMImageInfo> all_images_for(const std::string& remote_name) override;
    void for_each_entry_do(const Action& action) override;

private:
    URLDownloader* const url_downloader;
    std::unordered_map<std::string, std::unique_ptr<CustomManifest>> custom_image_info;
};
} // namespace multipass
#endif // MULTIPASS_CUSTOM_IMAGE_HOST

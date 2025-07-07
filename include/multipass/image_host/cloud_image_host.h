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

#include <multipass/disabled_copy_move.h>
#include <multipass/image_host/cloud_image_remote.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/vm_image_info.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace multipass
{
class CloudImageHost : private DisabledCopyMove
{
public:
    using Action = std::function<void(const std::string&, const VMImageInfo&)>;

    CloudImageHost(std::unordered_map<std::string, std::unique_ptr<CloudImageRemote>> remotes,
                   URLDownloader* downloader);
    virtual ~CloudImageHost() = default;

    virtual std::vector<VMImageInfo> all_images_for(const std::string& remote_name,
                                                    const bool allow_unsupported) const = 0;
    virtual std::vector<std::pair<std::string, VMImageInfo>> all_info_for(
        const Query& query) const = 0;
    virtual void fetch_remotes(const bool force_update) = 0;
    virtual void for_each_remote_do(const Action& action) = 0;
    virtual const CloudImageRemote& get_remote(const std::string& remote_name) const = 0;
    virtual std::optional<VMImageInfo> info_for(const Query& query) const = 0;
    virtual VMImageInfo info_for_full_hash(const std::string& full_hash) const = 0;
    virtual std::string url_from(const std::string& remote_name) const = 0;

private:
    std::unordered_map<std::string, std::unique_ptr<CloudImageRemote>> remotes;
    URLDownloader* const url_downloader;
};
} // namespace multipass

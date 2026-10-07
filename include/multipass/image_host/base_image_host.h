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

#include <multipass/image_host/vm_image_host.h>
#include <multipass/url_downloader.h>

#include <shared_mutex>

namespace multipass
{

class BaseVMImageHost : public VMImageHost
{
public:
    BaseVMImageHost(URLDownloader* downloader);

    std::optional<VMImageInfo> info_for(const SearchQuery& query) const final;
    std::vector<VMImageInfo> all_info_for(const SearchQuery& query) const final;
    VMImageInfo info_for_full_hash(const std::string& full_hash) const final;

    void for_each_entry_do(const Action& action) const final;
    void update_manifests(bool force_update) override;

protected:
    void on_manifest_update_failure(const std::string& details);
    void on_manifest_empty(const std::string& details);

    virtual const std::vector<VMImageInfo>* images_for_remote(const std::string& remote) const = 0;

    virtual void clear() = 0;
    virtual void fetch_manifests(bool force_update) = 0;

    mutable std::shared_mutex manifest_mutex;
    URLDownloader* const url_downloader;
};

} // namespace multipass

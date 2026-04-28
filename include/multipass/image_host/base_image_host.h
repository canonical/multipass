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

#include <QStringList>
#include <QTimer>

#include <shared_mutex>

namespace multipass
{

class BaseVMImageHost : public VMImageHost
{
public:
    BaseVMImageHost(URLDownloader* downloader);

    std::optional<VMImageInfo> info_for(const Query& query) const final;
    std::vector<std::pair<std::string, VMImageInfo>> all_info_for(const Query& query) const final;
    VMImageInfo info_for_full_hash(const std::string& full_hash) const final;
    std::vector<VMImageInfo> all_images_for(const std::string& remote_name,
                                            const bool allow_unsupported) const final;
    void for_each_entry_do(const Action& action) const final;
    void update_manifests(const bool force_update);

protected:
    void on_manifest_update_failure(const std::string& details);
    void on_manifest_empty(const std::string& details);

    virtual std::optional<VMImageInfo> info_for_impl(const Query& query) const = 0;
    virtual std::vector<std::pair<std::string, VMImageInfo>> all_info_for_impl(
        const Query& query) const = 0;
    virtual VMImageInfo info_for_full_hash_impl(const std::string& full_hash) const = 0;
    virtual std::vector<VMImageInfo> all_images_for_impl(const std::string& remote_name,
                                                         const bool allow_unsupported) const = 0;
    virtual void for_each_entry_do_impl(const Action& action) const = 0;
    virtual void clear() = 0;
    virtual void fetch_manifests(const bool force_update) = 0;

    mutable std::shared_mutex manifest_mutex;
    URLDownloader* const url_downloader;
};

} // namespace multipass

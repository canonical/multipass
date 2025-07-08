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

#include <multipass/url_downloader.h>
#include <multipass/vm_image_info.h>

#include <functional>
#include <optional>
#include <string>

#pragma once

namespace multipass
{
class CloudImageRemote
{
public:
    virtual ~CloudImageRemote() = default;

    virtual bool admits_image(const VMImageInfo& info) const = 0;
    virtual std::optional<std::string> get_mirror_url() const = 0;
    virtual std::string get_official_url() const = 0;
    virtual std::vector<VMImageInfo> get_products() const = 0;
    virtual std::vector<VMImageInfo> update_products(URLDownloader* downloader,
                                                     const bool force_update = false) = 0;
    virtual std::string get_url() const = 0;
    virtual const VMImageInfo* match_hash(const std::string hash) const = 0;
};
} // namespace multipass

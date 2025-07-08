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

#include <multipass/image_host/cloud_image_host.h>

#pragma once

namespace multipass
{
class BaseImageRemote : public CloudImageRemote
{
public:
    BaseImageRemote(std::string official_host,
                    std::string uri,
                    std::optional<std::string> mirror_key = std::nullopt);
    BaseImageRemote(std::string official_host,
                    std::string uri,
                    std::function<bool(const VMImageInfo&)> custom_image_admitter,
                    std::optional<std::string> mirror_key = std::nullopt);

    std::optional<std::string> get_mirror_url() const override;
    std::string get_official_url() const override;
    std::string get_url() const override;

private:
    static bool default_image_admitter(const VMImageInfo&);

    const std::string official_host;
    const std::string uri;
    const std::function<bool(const VMImageInfo&)> image_admitter;
    std::unordered_map<std::string, const VMImageInfo*> image_records;
    const std::optional<std::string> mirror_key;
    std::vector<VMImageInfo> products;
    std::string updated_at;
};
} // namespace multipass

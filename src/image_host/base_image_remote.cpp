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

#include <multipass/image_host/base_image_remote.h>
#include <multipass/settings/settings.h>

namespace mp = multipass;

namespace multipass
{
mp::BaseImageRemote::BaseImageRemote(std::string official_host,
                                     std::string uri,
                                     std::optional<std::string> mirror_key)
    : BaseImageRemote(std::move(official_host),
                      std::move(uri),
                      &default_image_admitter,
                      std::move(mirror_key))
{
}

mp::BaseImageRemote::BaseImageRemote(std::string official_host,
                                     std::string uri,
                                     std::function<bool(const VMImageInfo&)> custom_image_admitter,
                                     std::optional<std::string> mirror_key)
    : official_host(std::move(official_host)),
      uri(std::move(uri)),
      image_admitter{custom_image_admitter},
      mirror_key(std::move(mirror_key))
{
}

std::optional<std::string> mp::BaseImageRemote::get_mirror_url() const
{
    if (mirror_key)
    {
        auto mirror = MP_SETTINGS.get(QString::fromStdString(mirror_key.value()));
        if (!mirror.isEmpty())
        {
            auto url = mirror.toStdString();
            url.append(uri);
            return std::make_optional(url);
        }
    }

    return std::nullopt;
}

std::string mp::BaseImageRemote::get_official_url() const
{
    auto host = official_host;
    host.append(uri);
    return host;
}

std::string mp::BaseImageRemote::get_url() const
{
    return get_mirror_url().value_or(get_official_url());
}

bool mp::BaseImageRemote::default_image_admitter(const VMImageInfo&)
{
    return true;
}
} // namespace multipass

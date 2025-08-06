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

#include <multipass/image_host/multipass_image_host.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>

#include <fmt/format.h>

#include <utility>

namespace mp = multipass;
namespace
{
auto map_aliases_to_vm_info(const std::vector<mp::VMImageInfo>& images)
{
    std::unordered_map<std::string, const mp::VMImageInfo*> map;
    for (const auto& image : images)
    {
        map[image.id.toStdString()] = &image;
        for (const auto& alias : image.aliases)
        {
            map[alias.toStdString()] = &image;
        }
    }

    return map;
}

} // namespace

mp::MultipassManifest::MultipassManifest(std::vector<VMImageInfo>&& images)
    : products{std::move(images)}, image_records{map_aliases_to_vm_info(products)}
{
}

mp::MultipassVMImageHost::MultipassVMImageHost(URLDownloader* downloader)
    : BaseVMImageHost{downloader}, arch{QSysInfo::currentCpuArchitecture()}, manifest{}, remote{}
{
}

std::optional<mp::VMImageInfo> mp::MultipassVMImageHost::info_for(const Query& query)
{
    return {};
}

std::vector<std::pair<std::string, mp::VMImageInfo>> mp::MultipassVMImageHost::all_info_for(
    const Query& query)
{
    return {};
}

std::vector<mp::VMImageInfo> mp::MultipassVMImageHost::all_images_for(const std::string& remote_name,
                                                                   const bool allow_unsupported)
{
    return {};
}

std::vector<std::string> mp::MultipassVMImageHost::supported_remotes()
{
    return {};
}

void mp::MultipassVMImageHost::for_each_entry_do_impl(const Action& action)
{
}

mp::VMImageInfo mp::MultipassVMImageHost::info_for_full_hash_impl(const std::string& full_hash)
{
    return {};
}

void mp::MultipassVMImageHost::fetch_manifests(const bool force_update)
{
}

void mp::MultipassVMImageHost::clear()
{
}

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

#include <multipass/constants.h>
#include <multipass/exceptions/remote_not_found_exception.h>
#include <multipass/format.h>
#include <multipass/image_host/vm_image_host.h>
#include <multipass/query.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_vault.h>
#include <multipass/vm_image_vault_utils.h>

#include <string>
#include <utility>
#include <vector>

namespace multipass
{
class BaseVMImageVault : public VMImageVault
{
public:
    explicit BaseVMImageVault(const std::vector<VMImageHost*>& image_hosts)
        : image_hosts{image_hosts},
          remote_image_host_map{MP_IMAGE_VAULT_UTILS.configure_image_host_map(image_hosts)} {};

    VMImageHost* image_host_for(const std::string& remote_name) const override
    {
        auto it = remote_image_host_map.find(remote_name);
        return it == remote_image_host_map.end() ? nullptr : it->second;
    }

    VMImageHost& get_image_host_for(const std::string& remote_name) const
    {
        auto* host = image_host_for(remote_name);
        if (host == nullptr)
        {
            throw RemoteNotFoundException(remote_name);
        }

        return *host;
    }

    std::optional<VMImageInfo> any_info_for(const SearchQuery& query) const override
    {
        if (!query.remote_name.empty())
        {
            return get_image_host_for(query.remote_name).info_for(query);
        }

        // Not super obvious, but this falls back on the default remotes when none was specified.
        for (const auto& remote : default_remotes)
        {
            if (const auto* host = image_host_for(remote))
            {
                auto query_for_remote = query;
                query_for_remote.remote_name = remote;

                if (const auto info = host->info_for(query_for_remote))
                {
                    return info;
                }
            }
        }

        return std::nullopt;
    }

    std::vector<VMImageInfo> all_info_for(const SearchQuery& query) const final
    {
        return get_image_host_for(query.remote_name).all_info_for(query);
    }

    std::vector<std::string> fetch_remotes() const override
    {
        std::vector<std::string> remotes;
        for (const auto& [name, host] : remote_image_host_map)
        {
            remotes.emplace_back(name);
        }
        return remotes;
    }

private:
    std::vector<VMImageHost*> image_hosts;
    std::unordered_map<std::string, VMImageHost*> remote_image_host_map;
};
} // namespace multipass

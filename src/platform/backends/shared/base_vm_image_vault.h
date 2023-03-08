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

#ifndef MULTIPASS_BASE_VM_IMAGE_VAULT_H
#define MULTIPASS_BASE_VM_IMAGE_VAULT_H

#include <multipass/format.h>
#include <multipass/query.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <string>
#include <utility>
#include <vector>

namespace multipass
{
class BaseVMImageVault : public VMImageVault
{
public:
    explicit BaseVMImageVault(const std::vector<VMImageHost*>& image_hosts)
        : image_hosts{image_hosts}, remote_image_host_map{vault::configure_image_host_map(image_hosts)} {};

    VMImageHost* image_host_for(const std::string& remote_name) const override
    {
        auto it = remote_image_host_map.find(remote_name);
        if (it == remote_image_host_map.end())
        {
            throw std::runtime_error(
                fmt::format("Remote \'{}\' is not found. Please use `multipass find` for supported remotes and images.",
                            remote_name));
        }

        return it->second;
    };

    std::vector<std::pair<std::string, VMImageInfo>> all_info_for(const Query& query) const override
    {
        std::vector<std::pair<std::string, VMImageInfo>> images_info;

        auto grab_imgs = [&images_info, &query](auto* image_host) {
            return !(images_info = image_host->all_info_for(query)).empty();
        };

        if (!query.remote_name.empty())
            images_info = image_host_for(query.remote_name)->all_info_for(query);
        else
            static_cast<void>(std::any_of(image_hosts.begin(), image_hosts.end(), grab_imgs)); // intentional discard

        return images_info;
    };

protected:
    virtual std::optional<VMImageInfo> info_for(const Query& query) const
    {
        std::optional<VMImageInfo> info;

        if (!query.remote_name.empty())
        {
            auto image_host = image_host_for(query.remote_name);
            info = image_host->info_for(query);
        }
        else
        {
            for (const auto& image_host : image_hosts)
            {
                info = image_host->info_for(query);

                if (info)
                    break;
            }
        }

        return info;
    };

private:
    std::vector<VMImageHost*> image_hosts;
    std::unordered_map<std::string, VMImageHost*> remote_image_host_map;
};
} // namespace multipass

#endif // MULTIPASS_BASE_VM_IMAGE_VAULT_H

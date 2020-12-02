/*
 * Copyright (C) 2020 Canonical, Ltd.
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
#include <vector>

namespace multipass
{
class BaseVMImageVault : public VMImageVault
{
public:
    BaseVMImageVault(std::vector<VMImageHost*> image_hosts)
        : image_hosts{image_hosts}, remote_image_host_map{vault::configure_image_host_map(image_hosts)} {};

    VMImageHost* image_host_for(const std::string& remote_name) override
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

protected:
    virtual VMImageInfo info_for(const Query& query)
    {
        if (!query.remote_name.empty())
        {
            auto image_host = image_host_for(query.remote_name);
            auto info = image_host->info_for(query);

            if (info != nullopt)
                return *info;
        }
        else
        {
            for (const auto& image_host : image_hosts)
            {
                auto info = image_host->info_for(query);

                if (info)
                {
                    return *info;
                }
            }
        }

        throw std::runtime_error(fmt::format("Unable to find an image matching \"{}\"", query.release));
    };

private:
    std::vector<VMImageHost*> image_hosts;
    std::unordered_map<std::string, VMImageHost*> remote_image_host_map;
};
} // namespace multipass

#endif // MULTIPASS_BASE_VM_IMAGE_VAULT_H

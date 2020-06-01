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

#ifndef MULTIPASS_LXD_VM_IMAGE_VAULT_H
#define MULTIPASS_LXD_VM_IMAGE_VAULT_H

#include "lxd_request.h"

#include <multipass/network_access_manager.h>
#include <multipass/query.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <QUrl>

#include <memory>

namespace multipass
{
class LXDVMImageVault final : public VMImageVault
{
public:
    LXDVMImageVault(std::vector<VMImageHost*> image_host, const QUrl& base_url = lxd_socket_url);

    VMImage fetch_image(const FetchType& fetch_type, const Query& query, const PrepareAction& prepare,
                        const ProgressMonitor& monitor) override;
    void remove(const std::string& name) override;
    bool has_record_for(const std::string& name) override;
    void prune_expired_images() override;
    void update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                       const ProgressMonitor& monitor) override;

private:
    VMImageInfo info_for(const Query& query);

    std::vector<VMImageHost*> image_hosts;
    const QUrl base_url;
    std::unique_ptr<NetworkAccessManager> manager;
    std::unordered_map<std::string, VMImageHost*> remote_image_host_map;
};
} // namespace multipass
#endif // MULTIPASS_LXD_VM_IMAGE_VAULT_H

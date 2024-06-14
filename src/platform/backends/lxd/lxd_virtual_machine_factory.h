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

#ifndef MULTIPASS_LXD_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_LXD_VIRTUAL_MACHINE_FACTORY_H

#include "lxd_request.h"

#include <multipass/network_access_manager.h>
#include <shared/base_virtual_machine_factory.h>

#include <QUrl>

namespace multipass
{
class LXDVirtualMachineFactory : public BaseVirtualMachineFactory
{
public:
    explicit LXDVirtualMachineFactory(const Path& data_dir, const QUrl& base_url = lxd_socket_url);
    explicit LXDVirtualMachineFactory(NetworkAccessManager::UPtr manager, const Path& data_dir,
                                      const QUrl& base_url = lxd_socket_url);

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                const SSHKeyProvider& key_provider,
                                                VMStatusMonitor& monitor) override;
    VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image, const VirtualMachineDescription& desc) override;
    void hypervisor_health_check() override;
    QString get_backend_directory_name() const override
    {
        return "lxd";
    };
    QString get_backend_version_string() const override;
    VMImageVault::UPtr create_image_vault(std::vector<VMImageHost*> image_hosts, URLDownloader* downloader,
                                          const Path& cache_dir_path, const Path& data_dir_path,
                                          const days& days_to_expire) override;
    void configure(VirtualMachineDescription& vm_desc) override;
    std::vector<NetworkInterfaceInfo> networks() const override;
    void require_suspend_support() const override;

protected:
    void remove_resources_for_impl(const std::string& name) override;
    std::string create_bridge_with(const NetworkInterfaceInfo& interface) override;

private:
    NetworkAccessManager::UPtr manager;
    const QUrl base_url;
    QString storage_pool;
};
} // namespace multipass

inline void multipass::LXDVirtualMachineFactory::require_suspend_support() const
{
    throw NotImplementedOnThisBackendException{"suspend"};
}

#endif // MULTIPASS_LXD_VIRTUAL_MACHINE_FACTORY_H

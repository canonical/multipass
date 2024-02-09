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

#ifndef MULTIPASS_BASE_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_BASE_VIRTUAL_MACHINE_FACTORY_H

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/mount_handler.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include <daemon/default_vm_image_vault.h>

namespace multipass
{
constexpr auto log_category = "base factory";

class BaseVirtualMachineFactory : public VirtualMachineFactory
{
public:
    explicit BaseVirtualMachineFactory(const Path& instances_dir);
    VirtualMachine::UPtr create_vm_and_instance_disk_data(const QString& data_directory,
                                                          const VMSpecs& src_vm_spec,
                                                          const VMSpecs& dest_vm_spec,
                                                          const std::string& source_name,
                                                          const std::string& destination_name,
                                                          const VMImage& dest_vm_image,
                                                          VMStatusMonitor& monitor) override
    {
        throw NotImplementedOnThisBackendException("clone");
    }

    void remove_resources_for(const std::string& name) final;

    FetchType fetch_type() override
    {
        return FetchType::ImageOnly;
    };

    QString get_backend_directory_name() const override
    {
        return {};
    };

    Path get_instance_directory(const std::string& name) const override
    {
        return utils::backend_directory_path(instances_dir, QString::fromStdString(name));
    }

    void prepare_networking(std::vector<NetworkInterface>& extra_interfaces) override;

    VMImageVault::UPtr create_image_vault(std::vector<VMImageHost*> image_hosts, URLDownloader* downloader,
                                          const Path& cache_dir_path, const Path& data_dir_path,
                                          const days& days_to_expire) override
    {
        return std::make_unique<DefaultVMImageVault>(image_hosts, downloader, cache_dir_path, data_dir_path,
                                                     days_to_expire);
    };

    void configure(VirtualMachineDescription& vm_desc) override;

    std::vector<NetworkInterfaceInfo> networks() const override
    {
        throw NotImplementedOnThisBackendException("networks");
    };

    void require_snapshots_support() const override;

    void require_suspend_support() const override;

    void require_clone_support() const override
    {
        throw NotImplementedOnThisBackendException{"clone"};
    }

protected:
    static const Path instances_subdir;

protected:
    std::string create_bridge_with(const NetworkInterfaceInfo& interface) override
    {
        throw NotImplementedOnThisBackendException{"bridge creation"};
    }

    virtual void prepare_interface(NetworkInterface& net, std::vector<NetworkInterfaceInfo>& host_nets);

    virtual void remove_resources_for_impl(const std::string& name) = 0;

private:
    Path instances_dir;
};
} // namespace multipass

inline void multipass::BaseVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    remove_resources_for_impl(name);
    QDir instance_dir{get_instance_directory(name)};
    instance_dir.removeRecursively();
}

inline void multipass::BaseVirtualMachineFactory::require_snapshots_support() const
{
    throw NotImplementedOnThisBackendException{"snapshots"};
}

inline void multipass::BaseVirtualMachineFactory::require_suspend_support() const
{
}

#endif // MULTIPASS_BASE_VIRTUAL_MACHINE_FACTORY_H

/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <hyperv_api/hcs_virtual_machine_factory.h>

#include <shared/base_virtual_machine_factory.h>

#include <mutex>
#include <unordered_set>

namespace multipass::hyperv
{
class MigratingHyperVVirtualMachineFactory final : public BaseVirtualMachineFactory
{
public:
    MigratingHyperVVirtualMachineFactory(const Path& data_dir,
                                         AvailabilityZoneManager& az_manager);

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                const SSHKeyProvider& key_provider,
                                                VMStatusMonitor& monitor) override;
    void prepare_networking(std::vector<NetworkInterface>& extra_interfaces) override;
    VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image,
                                const VirtualMachineDescription& desc) override;
    void hypervisor_health_check() override;
    QString get_backend_version_string() const override
    {
        return "hyperv";
    }
    std::vector<NetworkInterfaceInfo> networks() const override;

protected:
    std::string create_bridge_with(const NetworkInterfaceInfo& interface) override;
    void remove_resources_for_impl(const std::string& name) override;

private:
    VirtualMachine::UPtr clone_vm_impl(const std::string& source_vm_name,
                                       const VMSpecs& src_vm_specs,
                                       const VirtualMachineDescription& desc,
                                       VMStatusMonitor& monitor,
                                       const SSHKeyProvider& key_provider) override;

    HCSVirtualMachineFactory hcs_factory;
    std::mutex pending_instances_mutex;
    std::unordered_set<std::string> pending_hcs_instances;
};
} // namespace multipass::hyperv

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

#include <shared/base_virtual_machine_factory.h>

namespace multipass
{
namespace test
{
struct HyperVNetworkAccessor; // fwd declaration, to befriend below
}

class HyperVVirtualMachineFactory final : public BaseVirtualMachineFactory
{
public:
    explicit HyperVVirtualMachineFactory(const Path& data_dir);

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                const SSHKeyProvider& key_provider,
                                                VMStatusMonitor& monitor) override;
    VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image,
                                const VirtualMachineDescription& desc) override;
    void hypervisor_health_check() override;
    QString get_backend_version_string() const override
    {
        return "hyperv";
    };
    std::vector<NetworkInterfaceInfo> networks() const override;
    void require_snapshots_support() const override; // TODO: remove after LXD migration
    void require_clone_support() const override      // TODO: remove after LXD migration
    {
    }

protected:
    std::string create_bridge_with(const NetworkInterfaceInfo& interface) override;
    void remove_resources_for_impl(const std::string& name) override;

private:
    friend struct multipass::test::HyperVNetworkAccessor;

    VirtualMachine::UPtr clone_vm_impl(const std::string& source_vm_name,
                                       const multipass::VMSpecs& src_vm_specs,
                                       const VirtualMachineDescription& desc,
                                       VMStatusMonitor& monitor,
                                       const SSHKeyProvider& key_provider) override;
    static std::vector<NetworkInterfaceInfo> get_switches(
        const std::vector<NetworkInterfaceInfo>& adapters);
    static std::vector<NetworkInterfaceInfo> get_adapters();
};
} // namespace multipass

inline void multipass::HyperVVirtualMachineFactory::require_snapshots_support() const
{
    // TODO: remove after LXD migration
}

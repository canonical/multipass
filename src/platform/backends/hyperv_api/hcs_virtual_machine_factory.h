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

#include <hyperv_api/hyperv_api_wrapper_fwdecl.h>

#include <shared/base_virtual_machine_factory.h>

namespace multipass::hyperv
{

/**
 * Native Windows virtual machine implementation using HCS, HCN & virtdisk API's.
 */
struct HCSVirtualMachineFactory final : public BaseVirtualMachineFactory
{

    explicit HCSVirtualMachineFactory(const Path& data_dir);
    explicit HCSVirtualMachineFactory(const Path& data_dir,
                                      hcs_sptr_t hcs,
                                      hcn_sptr_t hcn,
                                      virtdisk_sptr_t virtdisk);

    [[nodiscard]] VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                              const SSHKeyProvider& key_provider,
                                                              VMStatusMonitor& monitor) override;

    [[nodiscard]] VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image,
                                const VirtualMachineDescription& desc) override;
    void hypervisor_health_check() override
    {
    }

    [[nodiscard]] QString get_backend_version_string() const override
    {
        return "hyperv_api";
    };

    [[nodiscard]] std::vector<NetworkInterfaceInfo> networks() const override;

protected:
    [[nodiscard]] std::string create_bridge_with(const NetworkInterfaceInfo& interface) override;
    void remove_resources_for_impl(const std::string& name) override;

private:
    [[nodiscard]] VirtualMachine::UPtr clone_vm_impl(const std::string& source_vm_name,
                                                     const multipass::VMSpecs& src_vm_specs,
                                                     const VirtualMachineDescription& desc,
                                                     VMStatusMonitor& monitor,
                                                     const SSHKeyProvider& key_provider) override;

    hcs_sptr_t hcs_sptr{nullptr};
    hcn_sptr_t hcn_sptr{nullptr};
    virtdisk_sptr_t virtdisk_sptr{nullptr};

    /**
     * Retrieve a list of available network adapters.
     */
    [[nodiscard]] static std::vector<NetworkInterfaceInfo> get_adapters();
};
} // namespace multipass::hyperv

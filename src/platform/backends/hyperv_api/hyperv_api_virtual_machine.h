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

#ifndef MULTIPASS_HYPERV_API_HYPERV_VIRTUAL_MACHINE_H
#define MULTIPASS_HYPERV_API_HYPERV_VIRTUAL_MACHINE_H

#include "hcn/hyperv_hcn_api_wrapper.h"
#include "hcs/hyperv_hcs_api_wrapper.h"
#include "virtdisk/virtdisk_api_wrapper.h"
#include <multipass/virtual_machine_description.h>
#include <shared/base_virtual_machine.h>

#include <memory>

namespace multipass
{
class VMStatusMonitor;
}

namespace multipass::hyperv
{

/**
 * Native Windows virtual machine implementation using HCS, HCN & virtdisk API's.
 */
struct HyperVAPIVirtualMachine final : public BaseVirtualMachine
{
    using unique_hcn_wrapper_t = std::unique_ptr<hcn::HCNWrapperInterface>;
    using unique_hcs_wrapper_t = std::unique_ptr<hcs::HCSWrapperInterface>;
    using unique_virtdisk_wrapper_t = std::unique_ptr<virtdisk::VirtDiskWrapperInterface>;

    HyperVAPIVirtualMachine(unique_hcs_wrapper_t hcs_w,
                            unique_hcn_wrapper_t hcn_w,
                            unique_virtdisk_wrapper_t virtdisk_w,
                            const std::string& network_guid,
                            const VirtualMachineDescription& desc,
                            VMStatusMonitor& monitor,
                            const SSHKeyProvider& key_provider,
                            const Path& instance_dir);

    HyperVAPIVirtualMachine(unique_hcs_wrapper_t hcs_w,
                            unique_hcn_wrapper_t hcn_w,
                            unique_virtdisk_wrapper_t virtdisk_w,
                            const std::string& source_vm_name,
                            const multipass::VMSpecs& src_vm_specs,
                            const VirtualMachineDescription& desc,
                            VMStatusMonitor& monitor,
                            const SSHKeyProvider& key_provider,
                            const Path& dest_instance_dir);

    void start() override;
    void shutdown(ShutdownPolicy shutdown_policy = ShutdownPolicy::Powerdown) override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::string management_ipv4() override;
    std::string ipv6() override;
    void ensure_vm_is_running() override;
    void update_state() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;
    void add_network_interface(int index,
                               const std::string& default_mac_addr,
                               const NetworkInterface& extra_interface) override;
    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target, const VMMount& mount) override;

protected:
    void require_snapshots_support() const override{}

private:
    const VirtualMachineDescription description;
    unique_hcs_wrapper_t hcs{nullptr};
    unique_hcn_wrapper_t hcn{nullptr};
    unique_virtdisk_wrapper_t virtdisk{nullptr};

    VMStatusMonitor& monitor;

    hcs::ComputeSystemState fetch_state_from_api();
    void set_state(hcs::ComputeSystemState state);
};
} // namespace multipass::hyperv

#endif

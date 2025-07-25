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

#include <hyperv_api/hcs/hyperv_hcs_compute_system_state.h>
#include <hyperv_api/hcs/hyperv_hcs_system_handle.h>
#include <hyperv_api/hyperv_api_wrapper_fwdecl.h>

#include <multipass/signal.h>
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
struct HCSVirtualMachine final : public BaseVirtualMachine
{
    HCSVirtualMachine(hcs_sptr_t hcs_w,
                      hcn_sptr_t hcn_w,
                      virtdisk_sptr_t virtdisk_w,
                      const std::string& network_guid,
                      const VirtualMachineDescription& desc,
                      VMStatusMonitor& monitor,
                      const SSHKeyProvider& key_provider,
                      const Path& instance_dir);

    HCSVirtualMachine(hcs_sptr_t hcs_w,
                      hcn_sptr_t hcn_w,
                      virtdisk_sptr_t virtdisk_w,
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
    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target,
                                                            const VMMount& mount) override;

protected:
    void require_snapshots_support() const override
    {
    }

    std::shared_ptr<Snapshot> make_specific_snapshot(const QString& filename) override;
    std::shared_ptr<Snapshot> make_specific_snapshot(const std::string& snapshot_name,
                                                     const std::string& comment,
                                                     const std::string& instance_id,
                                                     const VMSpecs& specs,
                                                     std::shared_ptr<Snapshot> parent) override;

private:
    const VirtualMachineDescription description{};
    const std::string primary_network_guid{};
    hcs_sptr_t hcs{nullptr};
    hcn_sptr_t hcn{nullptr};
    virtdisk_sptr_t virtdisk{nullptr};
    VMStatusMonitor& monitor;

    hcs::HcsSystemHandle hcs_system{nullptr};
    Signal shutdown_signal{};

    hcs::ComputeSystemState fetch_state_from_api();
    void set_state(hcs::ComputeSystemState state);

    /**
     * Create the compute system if it's not already present.
     *
     * @return true The compute system was absent and created
     * @return false The compute system is already present
     */
    bool maybe_create_compute_system() noexcept(false);

    /**
     * Retrieve path to the primary disk symbolic link
     */
    std::filesystem::path get_primary_disk_path() const noexcept(false);

    void grant_access_to_paths(std::list<std::filesystem::path> paths) const;

    static void compute_system_event_callback(void* event, void* context);
};
} // namespace multipass::hyperv

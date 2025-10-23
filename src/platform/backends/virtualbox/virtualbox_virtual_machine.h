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

#include <shared/base_virtual_machine.h>

#include <multipass/ip_address.h>
#include <multipass/path.h>
#include <multipass/virtual_machine_description.h>

#include <QString>

namespace multipass
{
class PowerShell;
class SSHKeyProvider;
class VirtualMachineDescription;
class VMStatusMonitor;

class VirtualBoxVirtualMachine final : public BaseVirtualMachine
{
public:
    VirtualBoxVirtualMachine(const VirtualMachineDescription& desc,
                             VMStatusMonitor& monitor,
                             const SSHKeyProvider& key_provider,
                             const Path& instance_dir);
    // Contruct the vm based on the source virtual machine
    VirtualBoxVirtualMachine(const std::string& source_vm_name,
                             const VirtualMachineDescription& desc,
                             VMStatusMonitor& monitor,
                             const SSHKeyProvider& key_provider,
                             const Path& dest_instance_dir);
    ~VirtualBoxVirtualMachine() override;

    void start() override;
    void shutdown(ShutdownPolicy shutdown_policy = ShutdownPolicy::Powerdown) override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::optional<std::string> management_ipv4() override;
    std::vector<std::string> get_all_ipv4() override;
    void ensure_vm_is_running() override;
    void update_state() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;
    void add_network_interface(int index,
                               const std::string& default_mac_addr,
                               const NetworkInterface& extra_interface) override;

protected:
    std::shared_ptr<Snapshot> make_specific_snapshot(const QString& filename) override;
    std::shared_ptr<Snapshot> make_specific_snapshot(const std::string& snapshot_name,
                                                     const std::string& comment,
                                                     const std::string& instance_id,
                                                     const VMSpecs& specs,
                                                     std::shared_ptr<Snapshot> parent) override;

private:
    VirtualBoxVirtualMachine(const VirtualMachineDescription& desc,
                             VMStatusMonitor& monitor,
                             const SSHKeyProvider& key_provider,
                             const Path& instance_dir_qstr,
                             bool is_internal);
    void remove_snapshots_from_backend() const;

    // TODO we should probably keep the VMDescription in the base VM class instead
    VirtualMachineDescription desc;
    const QString name;
    std::optional<int> port;
    VMStatusMonitor* monitor;
    bool update_suspend_status{true};
};
} // namespace multipass

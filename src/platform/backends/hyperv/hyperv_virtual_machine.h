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

#include <string>
#include <vector>

namespace multipass
{
struct NetworkInterface;
struct VMSpecs;
class PowerShell;
class SSHKeyProvider;
class VirtualMachineDescription;
class VMStatusMonitor;

class HyperVVirtualMachine final : public BaseVirtualMachine
{
public:
    HyperVVirtualMachine(const VirtualMachineDescription& desc,
                         VMStatusMonitor& monitor,
                         const SSHKeyProvider& key_provider,
                         const Path& instance_dir);
    // Contruct the vm based on the source virtual machine
    HyperVVirtualMachine(const std::string& source_vm_name,
                         const multipass::VMSpecs& src_vm_specs,
                         const VirtualMachineDescription& desc,
                         VMStatusMonitor& monitor,
                         const SSHKeyProvider& key_provider,
                         const Path& dest_instance_dir);
    ~HyperVVirtualMachine();
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
    void require_snapshots_support() const override;
    std::shared_ptr<Snapshot> make_specific_snapshot(const QString& filename) override;
    std::shared_ptr<Snapshot> make_specific_snapshot(const std::string& snapshot_name,
                                                     const std::string& comment,
                                                     const std::string& instance_id,
                                                     const VMSpecs& specs,
                                                     std::shared_ptr<Snapshot> parent) override;

private:
    HyperVVirtualMachine(const VirtualMachineDescription& desc,
                         VMStatusMonitor& monitor,
                         const SSHKeyProvider& key_provider,
                         const Path& instance_dir,
                         bool is_internal); // is_internal is a dummy parameter to differentiate
                                            // with other constructors

    void setup_network_interfaces();
    void update_network_interfaces(const VMSpecs& src_specs);
    void remove_snapshots_from_backend() const;

    VirtualMachineDescription desc; // TODO we should probably keep this in the base class instead
    const QString name;
    std::unique_ptr<PowerShell> power_shell;
    VMStatusMonitor* monitor;
    bool update_suspend_status{true};
};
} // namespace multipass

inline void multipass::HyperVVirtualMachine::require_snapshots_support() const
{
}

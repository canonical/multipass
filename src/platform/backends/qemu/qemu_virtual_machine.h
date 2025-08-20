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

#include "qemu_platform.h"

#include <shared/base_virtual_machine.h>
#include <multipass/block_device_info.h>

#include <multipass/network_interface.h>
#include <multipass/process/process.h>
#include <multipass/virtual_machine_description.h>

#include <QObject>
#include <QStringList>

#include <unordered_map>

namespace multipass
{
class QemuPlatform;
class VMStatusMonitor;

class QemuVirtualMachine : public QObject, public BaseVirtualMachine
{
    Q_OBJECT
public:
    using MountArgs = std::unordered_map<std::string, std::pair<std::string, QStringList>>;

    QemuVirtualMachine(const VirtualMachineDescription& desc,
                       QemuPlatform* qemu_platform,
                       VMStatusMonitor& monitor,
                       const SSHKeyProvider& key_provider,
                       const Path& instance_dir,
                       bool remove_snapshots = false);
    ~QemuVirtualMachine();

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
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void update_state() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;
    virtual void add_network_interface(int index,
                                       const std::string& default_mac_addr,
                                       const NetworkInterface& extra_interface) override;
    virtual MountArgs& modifiable_mount_args();
    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target,
                                                             const VMMount& mount) override;
                                                             
    // Block device operations
    void attach_block_device(const std::string& name, const BlockDeviceInfo& info);
    void detach_block_device(const std::string& name);
    bool has_block_device(const std::string& name) const;
signals:
    void on_delete_memory_snapshot();
    void on_reset_network();
    void on_synchronize_clock();

protected:
    // TODO remove this, the onus of composing a VM of stubs should be on the stub VMs
    QemuVirtualMachine(const std::string& name,
                       const SSHKeyProvider& key_provider,
                       const Path& instance_dir)
        : BaseVirtualMachine{name, key_provider, instance_dir}
    {
    }

    void require_snapshots_support() const override;
    std::shared_ptr<Snapshot> make_specific_snapshot(const QString& filename) override;
    std::shared_ptr<Snapshot> make_specific_snapshot(const std::string& snapshot_name,
                                                     const std::string& comment,
                                                     const std::string& instance_id,
                                                     const VMSpecs& specs,
                                                     std::shared_ptr<Snapshot> parent) override;

private:
    void on_started();
    void on_error();
    void on_shutdown();
    void on_suspend();
    void on_restart();
    void initialize_vm_process();

    void connect_vm_signals();
    void disconnect_vm_signals();
    void remove_snapshots_from_backend() const;
    void load_block_devices_from_metadata();

    VirtualMachineDescription desc;
    std::unique_ptr<Process> vm_process{nullptr};
    QemuPlatform* qemu_platform;
    VMStatusMonitor* monitor;
    MountArgs mount_args;
    std::string saved_error_msg;
    bool update_shutdown_status{true};
    bool is_starting_from_suspend{false};
    bool force_shutdown{false};
    std::mutex vm_signal_mutex;
    bool vm_signals_connected{false};
    std::chrono::steady_clock::time_point network_deadline;
    
    // Track attached block devices
    std::unordered_map<std::string, BlockDeviceInfo> attached_block_devices;
};
} // namespace multipass

inline void multipass::QemuVirtualMachine::require_snapshots_support() const
{
}

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

#include "hyperv_migrator.h"

#include <multipass/ip_address.h>
#include <multipass/virtual_machine.h>

#include <mutex>

namespace multipass::hyperv
{
class MigratingHyperVVirtualMachine final : public VirtualMachine
{
public:
    MigratingHyperVVirtualMachine(VirtualMachine::UPtr legacy_vm,
                                  std::unique_ptr<HyperVMigrator> migrator);

    void start() override;
    void shutdown(ShutdownPolicy shutdown_policy) override;
    void suspend() override;
    void set_available(bool available) override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname() override;
    std::string ssh_username() override;
    std::optional<IPAddress> management_ipv4() override;
    std::vector<IPAddress> get_all_ipv4() override;
    std::string ssh_exec(const std::string& cmd, bool whisper = false) override;
    std::unique_ptr<SSHProcess> ssh_exec_process(const std::string& cmd,
                                                 bool whisper = false) override;
    std::unique_ptr<SSHSession> new_ssh_session() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void wait_for_cloud_init(std::chrono::milliseconds timeout) override;
    void handle_state_update() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size, UserMessages& messages) override;
    void add_network_interface(int index,
                               const std::string& default_mac_addr,
                               const NetworkInterface& extra_interface) override;
    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target,
                                                            const VMMount& mount) override;

    SnapshotVista view_snapshots(SnapshotPredicate predicate = {}) const override;
    int get_num_snapshots() const override;
    std::shared_ptr<const Snapshot> get_snapshot(const std::string& name) const override;
    std::shared_ptr<const Snapshot> get_snapshot(int index) const override;
    std::shared_ptr<Snapshot> get_snapshot(const std::string& name) override;
    std::shared_ptr<Snapshot> get_snapshot(int index) override;
    std::shared_ptr<const Snapshot> take_snapshot(const VMSpecs& specs,
                                                  const std::string& snapshot_name,
                                                  const std::string& comment) override;
    void rename_snapshot(const std::string& old_name, const std::string& new_name) override;
    void delete_snapshot(const std::string& name) override;
    void restore_snapshot(const std::string& name, VMSpecs& specs) override;
    void load_snapshots() override;
    std::vector<std::string> get_childrens_names(const Snapshot* parent) const override;
    int get_snapshot_count() const override;

    QDir instance_directory() const override;
    const std::string& get_name() const override;
    const AvailabilityZone& get_zone() const override;

private:
    std::shared_ptr<VirtualMachine> delegate() const;
    void sync_state();

    std::shared_ptr<VirtualMachine> active_vm;
    std::unique_ptr<HyperVMigrator> migrator;
    const std::string name;
    const QDir instance_dir;
    const AvailabilityZone& zone;
    bool migration_committed{false};
    mutable std::mutex delegate_mutex;
    std::mutex migration_mutex;
};
} // namespace multipass::hyperv

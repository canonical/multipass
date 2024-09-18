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

#ifndef MULTIPASS_LXD_VIRTUAL_MACHINE_H
#define MULTIPASS_LXD_VIRTUAL_MACHINE_H

#include <QString>
#include <QUrl>

#include <shared/base_virtual_machine.h>

namespace multipass
{
class NetworkAccessManager;
class VirtualMachineDescription;
class VMStatusMonitor;

class LXDVirtualMachine : public BaseVirtualMachine
{
public:
    LXDVirtualMachine(const VirtualMachineDescription& desc,
                      VMStatusMonitor& monitor,
                      NetworkAccessManager* manager,
                      const QUrl& base_url,
                      const QString& bridge_name,
                      const QString& storage_pool,
                      const SSHKeyProvider& key_provider,
                      const Path& instance_dir);
    ~LXDVirtualMachine() override;

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
    void ensure_vm_is_running(const std::chrono::milliseconds& timeout);
    void update_state() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;
    void add_network_interface(int index,
                               const std::string& default_mac_addr,
                               const NetworkInterface& extra_interface) override;
    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target, const VMMount& mount) override;

private:
    void add_extra_interface_to_instance_cloud_init(const std::string& default_mac_addr,
                                                    const NetworkInterface& extra_interface) const override;
    void apply_extra_interfaces_and_instance_id_to_cloud_init(const std::string& default_mac_addr,
                                                              const std::vector<NetworkInterface>& extra_interfaces,
                                                              const std::string& new_instance_id) const override
    {
        throw NotImplementedOnThisBackendException("apply_extra_interfaces_and_instance_id_to_cloud_init");
    }

    const QString name;
    const std::string username;
    std::optional<int> port;
    VMStatusMonitor* monitor;
    bool update_shutdown_status{true};
    NetworkAccessManager* manager;
    const QUrl base_url;
    const QString bridge_name;
    const QString mac_addr;
    const QString storage_pool;

    const QUrl url() const;
    const QUrl state_url();
    const QUrl network_leases_url();
    void request_state(const QString& new_state, const QJsonObject& args = {});
};
} // namespace multipass
#endif // MULTIPASS_LXD_VIRTUAL_MACHINE_H

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

#ifndef MULTIPASS_HYPERV_VIRTUAL_MACHINE_H
#define MULTIPASS_HYPERV_VIRTUAL_MACHINE_H

#include <shared/base_virtual_machine.h>

#include <multipass/ip_address.h>
#include <multipass/path.h>

#include <QString>

#include <string>
#include <vector>

namespace multipass
{
struct NetworkInterface;
class PowerShell;
class SSHKeyProvider;
class VirtualMachineDescription;
class VMStatusMonitor;

class HyperVVirtualMachine final : public BaseVirtualMachine
{
public:
    HyperVVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor);
    ~HyperVVirtualMachine();
    void stop() override;
    void start() override;
    void shutdown() override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::string management_ipv4(const SSHKeyProvider& key_provider) override;
    std::string ipv6() override;
    void ensure_vm_is_running() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout, const SSHKeyProvider& key_provider) override;
    void update_state() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;
    std::unique_ptr<MountHandler> make_native_mount_handler(const SSHKeyProvider* ssh_key_provider,
                                                            const std::string& target, const VMMount& mount) override;

private:
    void setup_network_interfaces(const std::string& default_mac_address,
                                  const std::vector<NetworkInterface>& extra_interfaces);

    // TODO we should probably keep the VMDescription in the base VM class, instead of a few of these attributes
    const QString name;
    const std::string username;
    const Path image_path;
    std::optional<multipass::IPAddress> ip;
    std::unique_ptr<PowerShell> power_shell;
    VMStatusMonitor* monitor;
    bool update_suspend_status{true};
};
} // namespace multipass
#endif // MULTIPASS_HYPERV_VIRTUAL_MACHINE_H

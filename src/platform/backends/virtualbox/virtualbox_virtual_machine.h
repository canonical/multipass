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

#ifndef MULTIPASS_VIRTUALBOX_VIRTUAL_MACHINE_H
#define MULTIPASS_VIRTUALBOX_VIRTUAL_MACHINE_H

#include <shared/base_virtual_machine.h>

#include <multipass/ip_address.h>
#include <multipass/path.h>

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
    VirtualBoxVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor);
    ~VirtualBoxVirtualMachine() override;
    void stop() override;
    void start() override;
    void shutdown() override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::string management_ipv4(const SSHKeyProvider& key_provider) override;
    std::vector<std::string> get_all_ipv4(const SSHKeyProvider& key_provider) override;
    std::string ipv6() override;
    void ensure_vm_is_running() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout, const SSHKeyProvider& key_provider) override;
    void update_state() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;

private: // TODO we should probably keep the VMDescription in the base VM class, instead of a few of these attributes
    const QString name;
    const std::string username;
    const Path image_path;
    std::optional<int> port;
    VMStatusMonitor* monitor;
    bool update_suspend_status{true};
};
} // namespace multipass
#endif // MULTIPASS_VIRTUALBOX_VIRTUAL_MACHINE_H

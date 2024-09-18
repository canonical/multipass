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

#ifndef MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_H
#define MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_H

#include "libvirt_wrapper.h"

#include <shared/base_virtual_machine.h>

#include <multipass/virtual_machine_description.h>

namespace multipass
{
class VMStatusMonitor;

class LibVirtVirtualMachine final : public BaseVirtualMachine
{
public:
    using ConnectionUPtr = std::unique_ptr<virConnect, decltype(virConnectClose)*>;
    using DomainUPtr = std::unique_ptr<virDomain, decltype(virDomainFree)*>;
    using NetworkUPtr = std::unique_ptr<virNetwork, decltype(virNetworkFree)*>;

    LibVirtVirtualMachine(const VirtualMachineDescription& desc,
                          const std::string& bridge_name,
                          VMStatusMonitor& monitor,
                          const LibvirtWrapper::UPtr& libvirt_wrapper,
                          const SSHKeyProvider& key_provider,
                          const Path& instance_dir);
    ~LibVirtVirtualMachine();

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

    static ConnectionUPtr open_libvirt_connection(const LibvirtWrapper::UPtr& libvirt_wrapper);

private:
    DomainUPtr initialize_domain_info(virConnectPtr connection);
    DomainUPtr checked_vm_domain() const;

    std::string mac_addr;
    const std::string username;
    VirtualMachineDescription desc;
    VMStatusMonitor* monitor;
    // Make this a reference since LibVirtVirtualMachineFactory can modify the name later
    const std::string& bridge_name;
    // Needs to be a reference so testing can override the various libvirt functions
    const LibvirtWrapper::UPtr& libvirt_wrapper;
    bool update_suspend_status{true};
};
} // namespace multipass

#endif // MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_H

/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

    LibVirtVirtualMachine(const VirtualMachineDescription& desc, const std::string& bridge_name,
                          VMStatusMonitor& monitor, const LibvirtWrapper::UPtr& libvirt_wrapper);
    ~LibVirtVirtualMachine();

    void start() override;
    void stop(bool force = false) override;
    void shutdown(bool force = false) override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::string management_ipv4() override;
    std::string ipv6() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void ensure_vm_is_running() override;
    void update_state() override;

    static ConnectionUPtr open_libvirt_connection(const LibvirtWrapper::UPtr& libvirt_wrapper);

private:
    DomainUPtr initialize_domain_info(virConnectPtr connection);

    std::string mac_addr;
    const std::string username;
    const VirtualMachineDescription desc;
    VMStatusMonitor* monitor;
    // Make this a reference since LibVirtVirtualMachineFactory can modify the name later
    const std::string& bridge_name;
    // Needs to be a reference so testing can override the various libvirt functions
    const LibvirtWrapper::UPtr& libvirt_wrapper;
    bool update_suspend_status{true};
};
} // namespace multipass

#endif // MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_H

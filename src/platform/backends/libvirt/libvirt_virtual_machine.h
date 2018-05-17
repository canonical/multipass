/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/ip_address.h>
#include <multipass/optional.h>
#include <multipass/virtual_machine.h>

#include <libvirt/libvirt.h>

namespace multipass
{
class VMStatusMonitor;
class VirtualMachineDescription;

class LibVirtVirtualMachine final : public VirtualMachine
{
public:
    using DomainUPtr = std::unique_ptr<virDomain, decltype(virDomainFree)*>;
    using NetworkUPtr = std::unique_ptr<virNetwork, decltype(virNetworkFree)*>;

    LibVirtVirtualMachine(const VirtualMachineDescription& desc, virConnectPtr connection,
                          const std::string& bridge_name, VMStatusMonitor& monitor);
    ~LibVirtVirtualMachine();

    void start() override;
    void stop() override;
    void shutdown() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname() override;
    std::string ssh_username() override;
    std::string ipv4() override;
    std::string ipv6() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void wait_for_cloud_init(std::chrono::milliseconds timeout) override;

private:
    virConnectPtr connection;
    DomainUPtr domain;
    const std::string mac_addr;
    const std::string username;
    optional<IPAddress> ip;
    VMStatusMonitor* monitor;
};
}

#endif // MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_H

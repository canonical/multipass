/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_QEMU_VIRTUAL_MACHINE_H
#define MULTIPASS_QEMU_VIRTUAL_MACHINE_H

#include "dnsmasq_server.h"

#include <multipass/ip_address.h>
#include <multipass/virtual_machine.h>

#include <experimental/optional>

class QProcess;
class QFile;
class QString;

namespace multipass
{
class VMStatusMonitor;
class VirtualMachineDescription;

class QemuVirtualMachine final : public VirtualMachine
{
public:
    QemuVirtualMachine(const VirtualMachineDescription& desc, std::experimental::optional<IPAddress> address,
                       const std::string& tap_device_name, const std::string& mac_addr, DNSMasqServer& dnsmasq_server,
                       VMStatusMonitor& monitor);
    ~QemuVirtualMachine();

    void start() override;
    void stop() override;
    void shutdown() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname() override;
    std::string ipv4() override;
    std::string ipv6() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;

private:
    void on_started();
    void on_error();
    void on_shutdown();
    VirtualMachine::State state;
    std::experimental::optional<IPAddress> ip;
    const std::string tap_device_name;
    const std::string mac_addr;
    DNSMasqServer* dnsmasq_server;
    VMStatusMonitor* monitor;
    std::unique_ptr<QProcess> vm_process;
    std::string saved_error_msg;
};
}

#endif // MULTIPASS_QEMU_VIRTUAL_MACHINE_H

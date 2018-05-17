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

#include <multipass/ip_address.h>
#include <multipass/optional.h>
#include <multipass/virtual_machine.h>

class QProcess;
class QFile;
class QString;

namespace multipass
{
class DNSMasqServer;
class VMStatusMonitor;
class VirtualMachineDescription;

class QemuVirtualMachine final : public VirtualMachine
{
public:
    QemuVirtualMachine(const VirtualMachineDescription& desc, optional<IPAddress> address,
                       const std::string& tap_device_name, DNSMasqServer& dnsmasq_server, VMStatusMonitor& monitor);
    ~QemuVirtualMachine();

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
    void on_started();
    void on_error();
    void on_shutdown();
    void on_restart();
    void ensure_vm_is_running();
    optional<IPAddress> ip;
    bool is_legacy_ip;
    const std::string tap_device_name;
    const std::string mac_addr;
    const std::string username;
    DNSMasqServer* dnsmasq_server;
    VMStatusMonitor* monitor;
    std::unique_ptr<QProcess> vm_process;
    std::string saved_error_msg;
};
}

#endif // MULTIPASS_QEMU_VIRTUAL_MACHINE_H

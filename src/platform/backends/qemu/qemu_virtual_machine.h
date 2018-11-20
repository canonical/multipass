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
 */

#ifndef MULTIPASS_QEMU_VIRTUAL_MACHINE_H
#define MULTIPASS_QEMU_VIRTUAL_MACHINE_H

#include <multipass/ip_address.h>
#include <multipass/optional.h>
#include <multipass/virtual_machine.h>

#include <QStringList>

namespace multipass
{
class DNSMasqServer;
class ConfinementSystem;
class Process;
class VMStatusMonitor;
class VirtualMachineDescription;

class QemuVirtualMachine final : public VirtualMachine
{
public:
    QemuVirtualMachine(const std::shared_ptr<ConfinementSystem>& confinement_system,
                       const VirtualMachineDescription& desc, const std::string& tap_device_name,
                       DNSMasqServer& dnsmasq_server, VMStatusMonitor& monitor);
    ~QemuVirtualMachine();

    void start() override;
    void stop() override;
    void shutdown() override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname() override;
    std::string ssh_username() override;
    std::string ipv4() override;
    std::string ipv6() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void wait_for_cloud_init(std::chrono::milliseconds timeout) override;
    void update_state() override;

private:
    void on_started();
    void on_error();
    void on_shutdown();
    void on_suspend();
    void on_restart();
    void ensure_vm_is_running();
    multipass::optional<IPAddress> ip;
    const std::shared_ptr<ConfinementSystem> confinement_system;
    const std::string tap_device_name;
    const std::string mac_addr;
    const std::string username;
    DNSMasqServer* dnsmasq_server;
    VMStatusMonitor* monitor;
    const std::unique_ptr<Process> vm_process;
    std::string saved_error_msg;
    bool update_shutdown_status{true};
    bool delete_memory_snapshot{false};
};
} // namespace multipass

#endif // MULTIPASS_QEMU_VIRTUAL_MACHINE_H

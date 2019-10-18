/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_HYPERKIT_VIRTUAL_MACHINE_H
#define MULTIPASS_HYPERKIT_VIRTUAL_MACHINE_H

#include <multipass/ip_address.h>
#include <multipass/optional.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <QThread>

namespace multipass
{
class VMProcess;
class VMStatusMonitor;

class HyperkitVirtualMachine final : public VirtualMachine
{
public:
    HyperkitVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor);
    ~HyperkitVirtualMachine();

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
    void ensure_vm_is_running() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void update_state() override;

private:
    void on_start();
    void on_shutdown();
    VMStatusMonitor* monitor;
    std::unique_ptr<VMProcess> vm_process;
    const std::string username;
    QThread thread;
    multipass::optional<IPAddress> ip;
    const VirtualMachineDescription desc;
    bool update_shutdown_status;
};
}

#endif // MULTIPASS_HYPERKIT_VIRTUAL_MACHINE_H

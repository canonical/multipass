/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include <QJsonObject>
#include <QString>
#include <QUrl>

#include <shared/base_virtual_machine.h>

namespace multipass
{
class NetworkAccessManager;
class VirtualMachineDescription;
class VMStatusMonitor;

class LXDVirtualMachine final : public BaseVirtualMachine
{
public:
    LXDVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor, NetworkAccessManager* manager,
                      const QUrl& base_url, const QString& bridge_name, const QString& storage_pool);
    ~LXDVirtualMachine() override;
    void stop() override;
    void start() override;
    void shutdown() override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::string management_ipv4() override;
    std::string ipv6() override;
    void ensure_vm_is_running() override;
    void ensure_vm_is_running(const std::chrono::milliseconds& timeout);
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void update_state() override;

private:
    const QString name;
    const std::string username;
    multipass::optional<int> port;
    VMStatusMonitor* monitor;
    bool update_shutdown_status{true};
    NetworkAccessManager* manager;
    const QUrl base_url;
    const QString bridge_name;
    const QString mac_addr;

    const QUrl url();
    const QUrl state_url();
    const QUrl network_leases_url();
    void request_state(const QString& new_state);
    const multipass::optional<multipass::IPAddress> get_ip();
};
} // namespace multipass
#endif // MULTIPASS_LXD_VIRTUAL_MACHINE_H

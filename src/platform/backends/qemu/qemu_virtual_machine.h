/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

#include "qemu_platform.h"

#include <shared/base_virtual_machine.h>

#include <multipass/process/process.h>
#include <multipass/virtual_machine_description.h>

#include <QObject>
#include <QStringList>

#include <unordered_map>

namespace multipass
{
class QemuPlatform;
class VMStatusMonitor;

class QemuVirtualMachine final : public QObject, public BaseVirtualMachine
{
    Q_OBJECT
public:
    QemuVirtualMachine(const VirtualMachineDescription& desc, QemuPlatform* qemu_platform, VMStatusMonitor& monitor);
    ~QemuVirtualMachine();

    void start() override;
    void stop() override;
    void shutdown() override;
    void suspend() override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::string management_ipv4() override;
    std::string ipv6() override;
    void ensure_vm_is_running() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void update_state() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;
    std::unique_ptr<MountHandler> make_native_mount_handler(const SSHKeyProvider* ssh_key_provider, std::string target,
                                             const VMMount& mount) override;
    friend struct QemuMountHandler;

signals:
    void on_delete_memory_snapshot();
    void on_reset_network();

private:
    void on_started();
    void on_error();
    void on_shutdown();
    void on_suspend();
    void on_restart();
    void initialize_vm_process();

    VirtualMachineDescription desc;
    std::unique_ptr<Process> vm_process{nullptr};
    std::unordered_map<std::string, std::pair<std::string, QStringList>> mount_args;
    const std::string mac_addr;
    const std::string username;
    QemuPlatform* qemu_platform;
    VMStatusMonitor* monitor;
    std::string saved_error_msg;
    bool update_shutdown_status{true};
    bool is_starting_from_suspend{false};
    std::chrono::steady_clock::time_point network_deadline;
};
} // namespace multipass

#endif // MULTIPASS_QEMU_VIRTUAL_MACHINE_H

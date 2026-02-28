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

#pragma once

#include <applevz/applevz_wrapper.h>

#include <multipass/virtual_machine_description.h>
#include <shared/base_virtual_machine.h>

namespace multipass
{
class VMStatusMonitor;
}

namespace multipass::applevz
{
class AppleVZVirtualMachine : public BaseVirtualMachine
{
public:
    AppleVZVirtualMachine(const VirtualMachineDescription& desc,
                          VMStatusMonitor& monitor,
                          const SSHKeyProvider& key_provider,
                          const Path& instance_dir);
    ~AppleVZVirtualMachine();

    void start() override;
    void shutdown(ShutdownPolicy shutdown_policy = ShutdownPolicy::Powerdown) override;
    void suspend() override;

    State current_state() override;

    int ssh_port() override;
    std::string ssh_hostname(std::chrono::milliseconds timeout) override;
    std::string ssh_username() override;
    std::optional<IPAddress> management_ipv4() override;

    void handle_state_update() override;

    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void resize_disk(const MemorySize& new_size) override;

private:
    void initialize_vm_handle();
    void set_state(applevz::AppleVMState vm_state);
    void fetch_ip(std::chrono::milliseconds timeout);

private:
    VirtualMachineDescription desc;
    VMStatusMonitor* monitor;
    VMHandle vm_handle{nullptr};
};
} // namespace multipass::applevz

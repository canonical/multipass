/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <shared/base_virtual_machine.h>

#include <multipass/ip_address.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

namespace multipass::hyperv
{
class BlockedHyperVVirtualMachine final : public BaseVirtualMachine
{
public:
    BlockedHyperVVirtualMachine(const VirtualMachineDescription& description,
                                VMStatusMonitor& monitor,
                                const SSHKeyProvider& key_provider,
                                AvailabilityZone& zone,
                                const Path& instance_dir,
                                std::string reason);

    void start() override;
    void shutdown(ShutdownPolicy shutdown_policy) override;
    void suspend() override;
    void set_available(bool available) override;
    State current_state() override;
    int ssh_port() override;
    std::string ssh_hostname() override;
    std::string ssh_username() override;
    std::optional<IPAddress> management_ipv4() override;
    void handle_state_update() override;
    void update_cpus(int num_cores) override;
    void resize_memory(const MemorySize& new_size) override;
    void add_network_interface(int index,
                               const std::string& default_mac_addr,
                               const NetworkInterface& extra_interface) override;
    void load_snapshots() override;

protected:
    void resize_disk_impl(const MemorySize& new_size) override;

private:
    [[noreturn]] void throw_blocked() const;

    VMStatusMonitor& monitor;
    const std::string reason;
};
} // namespace multipass::hyperv

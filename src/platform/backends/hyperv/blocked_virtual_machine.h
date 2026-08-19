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

#include <multipass/constants.h>
#include <multipass/vm_status_monitor.h>
#include <shared/base_virtual_machine.h>

#include <fmt/format.h>

#include <stdexcept>
#include <utility>

namespace multipass::hyperv
{
class BlockedVirtualMachine final : public BaseVirtualMachine
{
public:
    BlockedVirtualMachine(const VirtualMachineDescription& description,
                          VMStatusMonitor& monitor,
                          const SSHKeyProvider& key_provider,
                          AvailabilityZone& zone,
                          const Path& instance_dir,
                          std::string reason)
        : BaseVirtualMachine{State::unavailable,
                             description.vm_name,
                             description,
                             key_provider,
                             zone,
                             instance_dir},
          monitor{monitor},
          reason{std::move(reason)}
    {
        handle_state_update();
    }

    void start() override
    {
        fail();
    }

    void shutdown(ShutdownPolicy) override
    {
        fail();
    }

    void suspend() override
    {
        fail();
    }

    void set_available(bool) override
    {
    }

    State current_state() override
    {
        return state = State::unavailable;
    }

    int ssh_port() override
    {
        return default_ssh_port;
    }

    std::string ssh_hostname() override
    {
        fail();
    }

    std::string ssh_username() override
    {
        return desc.ssh_username;
    }

    std::optional<IPAddress> management_ipv4() override
    {
        return std::nullopt;
    }

    void handle_state_update() override
    {
        monitor.persist_state_for(get_name(), State::unavailable);
    }

    void update_cpus(int) override
    {
        fail();
    }

    void resize_memory(const MemorySize&) override
    {
        fail();
    }

    void add_network_interface(int, const std::string&, const NetworkInterface&) override
    {
        fail();
    }

    void load_snapshots() override
    {
    }

private:
    void resize_disk_impl(const MemorySize&) override
    {
        fail();
    }

    [[noreturn]] void fail() const
    {
        throw std::runtime_error{
            fmt::format("Instance '{}' is blocked: {}", get_name(), reason)};
    }

    VMStatusMonitor& monitor;
    const std::string reason;
};
} // namespace multipass::hyperv

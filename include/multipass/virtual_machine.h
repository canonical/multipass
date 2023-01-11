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

#ifndef MULTIPASS_VIRTUAL_MACHINE_H
#define MULTIPASS_VIRTUAL_MACHINE_H

#include "disabled_copy_move.h"
#include "ip_address.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace multipass
{
class MemorySize;
class SSHKeyProvider;
struct VMMount;
class MountHandler;

class VirtualMachine : private DisabledCopyMove
{
public:
    enum class State
    {
        off,
        stopped,
        starting,
        restarting,
        running,
        delayed_shutdown,
        suspending,
        suspended,
        unknown
    };

    using UPtr = std::unique_ptr<VirtualMachine>;
    using ShPtr = std::shared_ptr<VirtualMachine>;

    virtual ~VirtualMachine() = default;
    virtual void stop() = 0;
    virtual void start() = 0;
    virtual void shutdown() = 0;
    virtual void suspend() = 0;
    virtual State current_state() = 0;
    virtual int ssh_port() = 0;
    virtual std::string ssh_hostname()
    {
        return ssh_hostname(std::chrono::minutes(2));
    };
    virtual std::string ssh_hostname(std::chrono::milliseconds timeout) = 0;
    virtual std::string ssh_username() = 0;
    virtual std::string management_ipv4() = 0;
    virtual std::vector<std::string> get_all_ipv4(const SSHKeyProvider& key_provider) = 0;
    virtual std::string ipv6() = 0;
    virtual void wait_until_ssh_up(std::chrono::milliseconds timeout) = 0;
    virtual void ensure_vm_is_running() = 0;
    virtual void update_state() = 0;
    virtual void update_cpus(int num_cores) = 0;
    virtual void resize_memory(const MemorySize& new_size) = 0;
    virtual void resize_disk(const MemorySize& new_size) = 0;
    virtual std::unique_ptr<MountHandler> make_native_mount_handler(const SSHKeyProvider* ssh_key_provider,
                                                                    const std::string& target,
                                                                    const VMMount& mount) = 0;

    VirtualMachine::State state;
    const std::string vm_name;
    std::condition_variable state_wait;
    std::mutex state_mutex;
    std::optional<IPAddress> management_ip;
    bool shutdown_while_starting{false};

protected:
    VirtualMachine(VirtualMachine::State state, const std::string& vm_name) : state{state}, vm_name{vm_name} {};
    VirtualMachine(const std::string& vm_name) : VirtualMachine(State::off, vm_name){};
};
} // namespace multipass
#endif // MULTIPASS_VIRTUAL_MACHINE_H

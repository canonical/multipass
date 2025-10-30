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

#include "disabled_copy_move.h"
#include "network_interface.h"

#include <QDir>
#include <QJsonObject>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace multipass
{
struct IPAddress;
class MemorySize;
class VMMount;
struct VMSpecs;
class MountHandler;
class Snapshot;

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

    enum class ShutdownPolicy
    {
        Powerdown, // gracefully shut down the vm
        Poweroff,  // forcefully shut down the vm
        Halt // halt the vm to non-running state. More specifically. suspended and stopped state
             // will remain the same and running state will be shut down to stopped state
    };

    using UPtr = std::unique_ptr<VirtualMachine>;
    using ShPtr = std::shared_ptr<VirtualMachine>;

    virtual ~VirtualMachine() = default;
    virtual void start() = 0;
    virtual void shutdown(ShutdownPolicy shutdown_policy = ShutdownPolicy::Powerdown) = 0;
    virtual void suspend() = 0;
    virtual State current_state() = 0;
    virtual int ssh_port() = 0;
    virtual std::string ssh_hostname()
    {
        return ssh_hostname(std::chrono::minutes(2));
    };
    virtual std::string ssh_hostname(std::chrono::milliseconds timeout) = 0;
    virtual std::string ssh_username() = 0;
    virtual std::optional<IPAddress> management_ipv4() = 0;
    virtual std::vector<IPAddress> get_all_ipv4() = 0;

    // careful: default param in virtual method; be sure to keep the same value in all descendants
    virtual std::string ssh_exec(const std::string& cmd, bool whisper = false) = 0;

    virtual void wait_until_ssh_up(std::chrono::milliseconds timeout) = 0;
    virtual void wait_for_cloud_init(std::chrono::milliseconds timeout) = 0;
    virtual void handle_state_update() = 0;
    virtual void update_cpus(int num_cores) = 0;
    virtual void resize_memory(const MemorySize& new_size) = 0;
    virtual void resize_disk(const MemorySize& new_size) = 0;
    virtual void add_network_interface(int index,
                                       const std::string& default_mac_addr,
                                       const NetworkInterface& extra_interface) = 0;
    virtual std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target,
                                                                    const VMMount& mount) = 0;

    using SnapshotVista = std::vector<std::shared_ptr<const Snapshot>>; // using vista to avoid
                                                                        // confusion with C++ views
    virtual SnapshotVista view_snapshots() const = 0;
    virtual int get_num_snapshots() const = 0;

    virtual std::shared_ptr<const Snapshot> get_snapshot(const std::string& name) const = 0;
    virtual std::shared_ptr<const Snapshot> get_snapshot(int index) const = 0;
    virtual std::shared_ptr<Snapshot> get_snapshot(const std::string& name) = 0;
    virtual std::shared_ptr<Snapshot> get_snapshot(int index) = 0;

    virtual std::shared_ptr<const Snapshot> take_snapshot(const VMSpecs& specs,
                                                          const std::string& snapshot_name,
                                                          const std::string& comment) = 0;
    virtual void rename_snapshot(
        const std::string& old_name,
        const std::string& new_name) = 0; // only VM can avoid repeated names
    virtual void delete_snapshot(const std::string& name) = 0;
    virtual void restore_snapshot(const std::string& name, VMSpecs& specs) = 0;
    virtual void load_snapshots() = 0;
    virtual std::vector<std::string> get_childrens_names(const Snapshot* parent) const = 0;
    virtual int get_snapshot_count() const = 0;

    virtual QDir instance_directory() const = 0;

    VirtualMachine::State state;
    const std::string vm_name;
    std::condition_variable state_wait;
    std::mutex state_mutex;

protected:
    explicit VirtualMachine(const std::string& vm_name) : VirtualMachine(State::off, vm_name)
    {
    }

    VirtualMachine(State state, const std::string& vm_name) : state{state}, vm_name{vm_name}
    {
    }
};
} // namespace multipass

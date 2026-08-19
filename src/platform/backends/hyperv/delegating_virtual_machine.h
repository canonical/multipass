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

#include <multipass/ip_address.h>
#include <multipass/mount_handler.h>
#include <multipass/ssh/ssh_process.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/virtual_machine.h>

#include <mutex>
#include <stdexcept>
#include <utility>

namespace multipass::hyperv
{
class DelegatingVirtualMachine : public VirtualMachine
{
public:
    explicit DelegatingVirtualMachine(VirtualMachine::UPtr delegate)
        : active_delegate{std::move(delegate)}
    {
        if (!active_delegate)
            throw std::invalid_argument{"A delegating VM requires a delegate"};

        name = active_delegate->get_name();
        instance_dir = active_delegate->instance_directory();
        zone = &active_delegate->get_zone();
        sync_state(active_delegate);
    }

    void start() override
    {
        const auto vm = delegate();
        try
        {
            vm->start();
        }
        catch (...)
        {
            sync_state(vm);
            throw;
        }
        sync_state(vm);
    }

    void shutdown(ShutdownPolicy policy) override
    {
        const auto vm = delegate();
        vm->shutdown(policy);
        sync_state(vm);
    }

    void suspend() override
    {
        const auto vm = delegate();
        vm->suspend();
        sync_state(vm);
    }

    void set_available(bool available) override
    {
        const auto vm = delegate();
        vm->set_available(available);
        sync_state(vm);
    }

    State current_state() override
    {
        const auto vm = delegate_or_null();
        return state = vm ? vm->current_state() : State::unknown;
    }

    int ssh_port() override
    {
        return delegate()->ssh_port();
    }

    std::string ssh_hostname() override
    {
        return delegate()->ssh_hostname();
    }

    std::string ssh_username() override
    {
        return delegate()->ssh_username();
    }

    std::optional<IPAddress> management_ipv4() override
    {
        return delegate()->management_ipv4();
    }

    std::vector<IPAddress> get_all_ipv4() override
    {
        return delegate()->get_all_ipv4();
    }

    std::string ssh_exec(const std::string& cmd, bool whisper = false) override
    {
        return delegate()->ssh_exec(cmd, whisper);
    }

    std::unique_ptr<SSHProcess> ssh_exec_process(const std::string& cmd,
                                                 bool whisper = false) override
    {
        return delegate()->ssh_exec_process(cmd, whisper);
    }

    std::unique_ptr<SSHSession> new_ssh_session() override
    {
        return delegate()->new_ssh_session();
    }

    void wait_until_ssh_up(std::chrono::milliseconds timeout) override
    {
        const auto vm = delegate();
        vm->wait_until_ssh_up(timeout);
        sync_state(vm);
    }

    void wait_for_cloud_init(std::chrono::milliseconds timeout) override
    {
        const auto vm = delegate();
        vm->wait_for_cloud_init(timeout);
        sync_state(vm);
    }

    void handle_state_update() override
    {
        const auto vm = delegate();
        vm->state = state;
        vm->handle_state_update();
        sync_state(vm);
    }

    void update_cpus(int cores) override
    {
        delegate()->update_cpus(cores);
    }

    void resize_memory(const MemorySize& size) override
    {
        delegate()->resize_memory(size);
    }

    void resize_disk(const MemorySize& size, UserMessages& messages) override
    {
        delegate()->resize_disk(size, messages);
    }

    void add_network_interface(int index,
                               const std::string& default_mac,
                               const NetworkInterface& interface) override
    {
        delegate()->add_network_interface(index, default_mac, interface);
    }

    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target,
                                                            const VMMount& mount) override
    {
        return delegate()->make_native_mount_handler(target, mount);
    }

    SnapshotVista view_snapshots(SnapshotPredicate predicate = {}) const override
    {
        return delegate()->view_snapshots(std::move(predicate));
    }

    int get_num_snapshots() const override
    {
        return delegate()->get_num_snapshots();
    }

    std::shared_ptr<const Snapshot> get_snapshot(const std::string& snapshot_name) const override
    {
        return delegate()->get_snapshot(snapshot_name);
    }

    std::shared_ptr<const Snapshot> get_snapshot(int index) const override
    {
        return delegate()->get_snapshot(index);
    }

    std::shared_ptr<Snapshot> get_snapshot(const std::string& snapshot_name) override
    {
        return delegate()->get_snapshot(snapshot_name);
    }

    std::shared_ptr<Snapshot> get_snapshot(int index) override
    {
        return delegate()->get_snapshot(index);
    }

    std::shared_ptr<const Snapshot> take_snapshot(const VMSpecs& specs,
                                                  const std::string& snapshot_name,
                                                  const std::string& comment) override
    {
        return delegate()->take_snapshot(specs, snapshot_name, comment);
    }

    void rename_snapshot(const std::string& old_name, const std::string& new_name) override
    {
        delegate()->rename_snapshot(old_name, new_name);
    }

    void delete_snapshot(const std::string& snapshot_name) override
    {
        delegate()->delete_snapshot(snapshot_name);
    }

    void restore_snapshot(const std::string& snapshot_name, VMSpecs& specs) override
    {
        delegate()->restore_snapshot(snapshot_name, specs);
    }

    void load_snapshots() override
    {
        delegate()->load_snapshots();
    }

    std::vector<std::string> get_childrens_names(const Snapshot* parent) const override
    {
        return delegate()->get_childrens_names(parent);
    }

    int get_snapshot_count() const override
    {
        return delegate()->get_snapshot_count();
    }

    QDir instance_directory() const override
    {
        return instance_dir;
    }

    const std::string& get_name() const override
    {
        return name;
    }

    const AvailabilityZone& get_zone() const override
    {
        return *zone;
    }

protected:
    std::shared_ptr<VirtualMachine> delegate() const
    {
        auto vm = delegate_or_null();
        if (!vm)
            throw std::runtime_error{"Virtual machine delegate is unavailable"};
        return vm;
    }

    std::shared_ptr<VirtualMachine> delegate_or_null() const
    {
        const std::lock_guard lock{delegate_mutex};
        return active_delegate;
    }

    void replace_delegate(VirtualMachine::UPtr replacement)
    {
        if (!replacement)
            throw std::invalid_argument{"Cannot replace a delegate with null"};

        const std::lock_guard lock{delegate_mutex};
        active_delegate = std::move(replacement);
    }

    void clear_delegate()
    {
        const std::lock_guard lock{delegate_mutex};
        active_delegate.reset();
    }

private:
    void sync_state(const std::shared_ptr<VirtualMachine>& vm)
    {
        state = vm->state;
    }

    mutable std::mutex delegate_mutex;
    std::shared_ptr<VirtualMachine> active_delegate;
    std::string name;
    QDir instance_dir;
    const AvailabilityZone* zone{};
};
} // namespace multipass::hyperv

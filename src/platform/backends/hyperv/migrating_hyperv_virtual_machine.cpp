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

#include "migrating_hyperv_virtual_machine.h"

#include <multipass/mount_handler.h>
#include <multipass/ssh/ssh_process.h>
#include <multipass/ssh/ssh_session.h>

#include <cassert>
#include <stdexcept>
#include <utility>

multipass::hyperv::MigratingHyperVVirtualMachine::MigratingHyperVVirtualMachine(
    VirtualMachine::UPtr legacy_vm,
    std::unique_ptr<HyperVMigrator> migrator)
    : active_vm{std::move(legacy_vm)},
      migrator{std::move(migrator)},
      name{active_vm->get_name()},
      instance_dir{active_vm->instance_directory()},
      zone{active_vm->get_zone()}
{
    assert(active_vm);
    assert(this->migrator);
    sync_state();
}

std::shared_ptr<multipass::VirtualMachine>
multipass::hyperv::MigratingHyperVVirtualMachine::delegate() const
{
    const std::lock_guard lock{delegate_mutex};
    if (!active_vm)
        throw std::runtime_error{
            "HCS delegate is unavailable after the legacy Hyper-V migration was committed"};

    return active_vm;
}

void multipass::hyperv::MigratingHyperVVirtualMachine::sync_state()
{
    const auto vm = delegate();
    state = vm->state;
}

void multipass::hyperv::MigratingHyperVVirtualMachine::start()
{
    const std::lock_guard migration_lock{migration_mutex};
    {
        const std::lock_guard lock{delegate_mutex};
        if (!active_vm)
            active_vm = migrator->make_target();
    }

    auto vm = delegate();
    const auto current = vm->current_state();
    if (!migration_committed && (current == State::off || current == State::stopped))
    {
        if (migrator->migrate(*vm))
        {
            migration_committed = true;
            {
                const std::lock_guard lock{delegate_mutex};
                active_vm.reset();
            }
            try
            {
                vm = migrator->make_target();
                const std::lock_guard lock{delegate_mutex};
                active_vm = vm;
            }
            catch (...)
            {
                state = State::unknown;
                throw;
            }
        }
    }

    try
    {
        delegate()->start();
        sync_state();
    }
    catch (...)
    {
        try
        {
            sync_state();
        }
        catch (const std::runtime_error&)
        {
        }
        throw;
    }
}

void multipass::hyperv::MigratingHyperVVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    delegate()->shutdown(shutdown_policy);
    sync_state();
}

void multipass::hyperv::MigratingHyperVVirtualMachine::suspend()
{
    delegate()->suspend();
    sync_state();
}

void multipass::hyperv::MigratingHyperVVirtualMachine::set_available(bool available)
{
    delegate()->set_available(available);
    sync_state();
}

multipass::VirtualMachine::State
multipass::hyperv::MigratingHyperVVirtualMachine::current_state()
{
    std::shared_ptr<VirtualMachine> vm;
    {
        const std::lock_guard lock{delegate_mutex};
        vm = active_vm;
    }
    if (!vm)
        return state = State::unknown;

    state = vm->current_state();
    return state;
}

int multipass::hyperv::MigratingHyperVVirtualMachine::ssh_port()
{
    return delegate()->ssh_port();
}

std::string multipass::hyperv::MigratingHyperVVirtualMachine::ssh_hostname()
{
    return delegate()->ssh_hostname();
}

std::string multipass::hyperv::MigratingHyperVVirtualMachine::ssh_username()
{
    return delegate()->ssh_username();
}

std::optional<multipass::IPAddress>
multipass::hyperv::MigratingHyperVVirtualMachine::management_ipv4()
{
    return delegate()->management_ipv4();
}

std::vector<multipass::IPAddress>
multipass::hyperv::MigratingHyperVVirtualMachine::get_all_ipv4()
{
    return delegate()->get_all_ipv4();
}

std::string multipass::hyperv::MigratingHyperVVirtualMachine::ssh_exec(const std::string& cmd,
                                                                       bool whisper)
{
    return delegate()->ssh_exec(cmd, whisper);
}

std::unique_ptr<multipass::SSHProcess>
multipass::hyperv::MigratingHyperVVirtualMachine::ssh_exec_process(const std::string& cmd,
                                                                   bool whisper)
{
    return delegate()->ssh_exec_process(cmd, whisper);
}

std::unique_ptr<multipass::SSHSession>
multipass::hyperv::MigratingHyperVVirtualMachine::new_ssh_session()
{
    return delegate()->new_ssh_session();
}

void multipass::hyperv::MigratingHyperVVirtualMachine::wait_until_ssh_up(
    std::chrono::milliseconds timeout)
{
    delegate()->wait_until_ssh_up(timeout);
    sync_state();
}

void multipass::hyperv::MigratingHyperVVirtualMachine::wait_for_cloud_init(
    std::chrono::milliseconds timeout)
{
    delegate()->wait_for_cloud_init(timeout);
    sync_state();
}

void multipass::hyperv::MigratingHyperVVirtualMachine::handle_state_update()
{
    const auto vm = delegate();
    vm->state = state;
    vm->handle_state_update();
    sync_state();
}

void multipass::hyperv::MigratingHyperVVirtualMachine::update_cpus(int num_cores)
{
    delegate()->update_cpus(num_cores);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::resize_memory(
    const MemorySize& new_size)
{
    delegate()->resize_memory(new_size);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::resize_disk(
    const MemorySize& new_size,
    UserMessages& messages)
{
    delegate()->resize_disk(new_size, messages);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::add_network_interface(
    int index,
    const std::string& default_mac_addr,
    const NetworkInterface& extra_interface)
{
    delegate()->add_network_interface(index, default_mac_addr, extra_interface);
}

std::unique_ptr<multipass::MountHandler>
multipass::hyperv::MigratingHyperVVirtualMachine::make_native_mount_handler(
    const std::string& target,
    const VMMount& mount)
{
    return delegate()->make_native_mount_handler(target, mount);
}

multipass::VirtualMachine::SnapshotVista
multipass::hyperv::MigratingHyperVVirtualMachine::view_snapshots(
    SnapshotPredicate predicate) const
{
    return delegate()->view_snapshots(std::move(predicate));
}

int multipass::hyperv::MigratingHyperVVirtualMachine::get_num_snapshots() const
{
    return delegate()->get_num_snapshots();
}

std::shared_ptr<const multipass::Snapshot>
multipass::hyperv::MigratingHyperVVirtualMachine::get_snapshot(const std::string& name) const
{
    return delegate()->get_snapshot(name);
}

std::shared_ptr<const multipass::Snapshot>
multipass::hyperv::MigratingHyperVVirtualMachine::get_snapshot(int index) const
{
    return delegate()->get_snapshot(index);
}

std::shared_ptr<multipass::Snapshot>
multipass::hyperv::MigratingHyperVVirtualMachine::get_snapshot(const std::string& name)
{
    return delegate()->get_snapshot(name);
}

std::shared_ptr<multipass::Snapshot>
multipass::hyperv::MigratingHyperVVirtualMachine::get_snapshot(int index)
{
    return delegate()->get_snapshot(index);
}

std::shared_ptr<const multipass::Snapshot>
multipass::hyperv::MigratingHyperVVirtualMachine::take_snapshot(
    const VMSpecs& specs,
    const std::string& snapshot_name,
    const std::string& comment)
{
    return delegate()->take_snapshot(specs, snapshot_name, comment);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::rename_snapshot(
    const std::string& old_name,
    const std::string& new_name)
{
    delegate()->rename_snapshot(old_name, new_name);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::delete_snapshot(const std::string& name)
{
    delegate()->delete_snapshot(name);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::restore_snapshot(const std::string& name,
                                                                        VMSpecs& specs)
{
    delegate()->restore_snapshot(name, specs);
}

void multipass::hyperv::MigratingHyperVVirtualMachine::load_snapshots()
{
    delegate()->load_snapshots();
}

std::vector<std::string>
multipass::hyperv::MigratingHyperVVirtualMachine::get_childrens_names(
    const Snapshot* parent) const
{
    return delegate()->get_childrens_names(parent);
}

int multipass::hyperv::MigratingHyperVVirtualMachine::get_snapshot_count() const
{
    return delegate()->get_snapshot_count();
}

QDir multipass::hyperv::MigratingHyperVVirtualMachine::instance_directory() const
{
    return instance_dir;
}

const std::string& multipass::hyperv::MigratingHyperVVirtualMachine::get_name() const
{
    return name;
}

const multipass::AvailabilityZone&
multipass::hyperv::MigratingHyperVVirtualMachine::get_zone() const
{
    return zone;
}

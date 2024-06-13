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

#ifndef MULTIPASS_MOCK_VIRTUAL_MACHINE_H
#define MULTIPASS_MOCK_VIRTUAL_MACHINE_H

#include "common.h"
#include "temp_dir.h"

#include <multipass/memory_size.h>
#include <multipass/mount_handler.h>
#include <multipass/virtual_machine.h>

#include <memory>

using namespace testing;

namespace multipass
{
namespace test
{
template <typename T = VirtualMachine, typename = std::enable_if_t<std::is_base_of_v<VirtualMachine, T>>>
struct MockVirtualMachineT : public T
{
    template <typename... Args>
    MockVirtualMachineT(Args&&... args) : MockVirtualMachineT{std::make_unique<TempDir>(), std::forward<Args>(args)...}
    {
    }

    template <typename... Args>
    MockVirtualMachineT(std::unique_ptr<TempDir>&& tmp_dir, Args&&... args)
        : T{std::forward<Args>(args)..., tmp_dir->path()}, tmp_dir{std::move(tmp_dir)}
    {
        ON_CALL(*this, current_state()).WillByDefault(Return(multipass::VirtualMachine::State::off));
        ON_CALL(*this, ssh_port()).WillByDefault(Return(42));
        ON_CALL(*this, ssh_hostname()).WillByDefault(Return("localhost"));
        ON_CALL(*this, ssh_hostname(_)).WillByDefault(Return("localhost"));
        ON_CALL(*this, ssh_username()).WillByDefault(Return("ubuntu"));
        ON_CALL(*this, management_ipv4()).WillByDefault(Return("0.0.0.0"));
        ON_CALL(*this, get_all_ipv4()).WillByDefault(Return(std::vector<std::string>{"192.168.2.123"}));
        ON_CALL(*this, ipv6()).WillByDefault(Return("::/0"));
    }

    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, shutdown, (VirtualMachine::ShutdownPolicy), (override));
    MOCK_METHOD(void, suspend, (), (override));
    MOCK_METHOD(multipass::VirtualMachine::State, current_state, (), (override));
    MOCK_METHOD(int, ssh_port, (), (override));
    MOCK_METHOD(std::string, ssh_hostname, (), (override));
    MOCK_METHOD(std::string, ssh_hostname, (std::chrono::milliseconds), (override));
    MOCK_METHOD(std::string, ssh_username, (), (override));
    MOCK_METHOD(std::string, management_ipv4, (), (override));
    MOCK_METHOD(std::vector<std::string>, get_all_ipv4, (), (override));
    MOCK_METHOD(std::string, ipv6, (), (override));

    MOCK_METHOD(std::string, ssh_exec, (const std::string& cmd, bool whisper), (override));
    std::string ssh_exec(const std::string& cmd)
    {
        return ssh_exec(cmd, false);
    }

    MOCK_METHOD(void, ensure_vm_is_running, (), (override));
    MOCK_METHOD(void, wait_until_ssh_up, (std::chrono::milliseconds), (override));
    MOCK_METHOD(void, wait_for_cloud_init, (std::chrono::milliseconds), (override));
    MOCK_METHOD(void, update_state, (), (override));
    MOCK_METHOD(void, update_cpus, (int), (override));
    MOCK_METHOD(void, resize_memory, (const MemorySize&), (override));
    MOCK_METHOD(void, resize_disk, (const MemorySize&), (override));
    MOCK_METHOD(void, add_network_interface, (int, const std::string&, const NetworkInterface&), (override));
    MOCK_METHOD(std::unique_ptr<MountHandler>,
                make_native_mount_handler,
                (const std::string&, const VMMount&),
                (override));
    MOCK_METHOD(VirtualMachine::SnapshotVista, view_snapshots, (), (const, override));
    MOCK_METHOD(int, get_num_snapshots, (), (const, override));
    MOCK_METHOD(std::shared_ptr<const Snapshot>, get_snapshot, (const std::string&), (const, override));
    MOCK_METHOD(std::shared_ptr<const Snapshot>, get_snapshot, (int index), (const, override));
    MOCK_METHOD(std::shared_ptr<Snapshot>, get_snapshot, (const std::string&), (override));
    MOCK_METHOD(std::shared_ptr<Snapshot>, get_snapshot, (int index), (override));
    MOCK_METHOD(std::shared_ptr<const Snapshot>,
                take_snapshot,
                (const VMSpecs&, const std::string&, const std::string&),
                (override));
    MOCK_METHOD(void, rename_snapshot, (const std::string& old_name, const std::string& new_name), (override));
    MOCK_METHOD(void, delete_snapshot, (const std::string& name), (override));
    MOCK_METHOD(void, restore_snapshot, (const std::string&, VMSpecs&), (override));
    MOCK_METHOD(void, load_snapshots, (), (override));
    MOCK_METHOD(std::vector<std::string>, get_childrens_names, (const Snapshot*), (const, override));
    MOCK_METHOD(int, get_snapshot_count, (), (const, override));

    std::unique_ptr<TempDir> tmp_dir;
};

using MockVirtualMachine = MockVirtualMachineT<>;
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_VIRTUAL_MACHINE_H

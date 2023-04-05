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

#include <multipass/memory_size.h>
#include <multipass/mount_handler.h>
#include <multipass/virtual_machine.h>

using namespace testing;

namespace multipass
{
namespace test
{
template <typename T = VirtualMachine, typename = std::enable_if_t<std::is_base_of_v<VirtualMachine, T>>>
struct MockVirtualMachineT : public T
{
    template <typename... Args>
    MockVirtualMachineT(Args&&... args) : T{std::forward<Args>(args)...}
    {
        ON_CALL(*this, current_state()).WillByDefault(Return(multipass::VirtualMachine::State::off));
        ON_CALL(*this, ssh_port()).WillByDefault(Return(42));
        ON_CALL(*this, ssh_hostname()).WillByDefault(Return("localhost"));
        ON_CALL(*this, ssh_hostname(_)).WillByDefault(Return("localhost"));
        ON_CALL(*this, ssh_username()).WillByDefault(Return("ubuntu"));
        ON_CALL(*this, management_ipv4(_)).WillByDefault(Return("0.0.0.0"));
        ON_CALL(*this, get_all_ipv4(_)).WillByDefault(Return(std::vector<std::string>{"192.168.2.123"}));
        ON_CALL(*this, ipv6()).WillByDefault(Return("::/0"));
    }

    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(void, shutdown, (), (override));
    MOCK_METHOD(void, suspend, (), (override));
    MOCK_METHOD(multipass::VirtualMachine::State, current_state, (), (override));
    MOCK_METHOD(int, ssh_port, (), (override));
    MOCK_METHOD(std::string, ssh_hostname, (), (override));
    MOCK_METHOD(std::string, ssh_hostname, (std::chrono::milliseconds), (override));
    MOCK_METHOD(std::string, ssh_username, (), (override));
    MOCK_METHOD(std::string, management_ipv4, (const SSHKeyProvider&), (override));
    MOCK_METHOD(std::vector<std::string>, get_all_ipv4, (const SSHKeyProvider&), (override));
    MOCK_METHOD(std::string, ipv6, (), (override));
    MOCK_METHOD(void, ensure_vm_is_running, (), (override));
    MOCK_METHOD(void, wait_until_ssh_up, (std::chrono::milliseconds, const SSHKeyProvider&), (override));
    MOCK_METHOD(void, update_state, (), (override));
    MOCK_METHOD(void, update_cpus, (int num_cores), (override));
    MOCK_METHOD(void, resize_memory, (const MemorySize& new_size), (override));
    MOCK_METHOD(void, resize_disk, (const MemorySize& new_size), (override));
    MOCK_METHOD(std::unique_ptr<MountHandler>, make_native_mount_handler,
                (const SSHKeyProvider* ssh_key_provider, const std::string& target, const VMMount& mount), (override));
    MOCK_METHOD(VirtualMachine::SnapshotVista, view_snapshots, (), (const, override, noexcept));
    MOCK_METHOD(std::shared_ptr<const Snapshot>, get_snapshot, (const std::string&), (const, override));
    MOCK_METHOD(std::shared_ptr<const Snapshot>, take_snapshot,
                (const QDir&, const VMSpecs& specs, const std::string& name, const std::string& comment), (override));
    MOCK_METHOD(void, load_snapshot, (const QJsonObject& json), (override));
};

using MockVirtualMachine = MockVirtualMachineT<>;
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_VIRTUAL_MACHINE_H

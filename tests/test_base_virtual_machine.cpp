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

#include "common.h"
#include "dummy_ssh_key_provider.h"
#include "file_operations.h"
#include "mock_logger.h"
#include "mock_ssh_test_fixture.h"
#include "mock_virtual_machine.h"
#include "multipass/exceptions/file_open_failed_exception.h"
#include "multipass/exceptions/snapshot_exceptions.h"
#include "multipass/logging/level.h"
#include "temp_dir.h"

#include <shared/base_virtual_machine.h>

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/snapshot.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/vm_specs.h>

#include <algorithm>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct MockBaseVirtualMachine : public mpt::MockVirtualMachineT<mp::BaseVirtualMachine>
{
    template <typename... Args>
    MockBaseVirtualMachine(Args&&... args)
        : mpt::MockVirtualMachineT<mp::BaseVirtualMachine>{std::forward<Args>(args)...}
    {
        auto& self = *this;
        const auto& const_self = self;
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, view_snapshots, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, get_num_snapshots, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, take_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, rename_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, delete_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, restore_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, load_snapshots, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, get_childrens_names, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(self, get_snapshot_count, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(self, get_snapshot, mp::BaseVirtualMachine, (An<int>()));
        MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(const_self, get_snapshot, mp::BaseVirtualMachine, (An<int>()));
        MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(self,
                                                     get_snapshot,
                                                     mp::BaseVirtualMachine,
                                                     (A<const std::string&>()));
        MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(const_self,
                                                     get_snapshot,
                                                     mp::BaseVirtualMachine,
                                                     (A<const std::string&>()));
    }

    MOCK_METHOD(void, require_snapshots_support, (), (const, override));
    MOCK_METHOD(std::shared_ptr<mp::Snapshot>, make_specific_snapshot, (const QString& filename), (override));
    MOCK_METHOD(std::shared_ptr<mp::Snapshot>,
                make_specific_snapshot,
                (const std::string& snapshot_name,
                 const std::string& comment,
                 const mp::VMSpecs& specs,
                 std::shared_ptr<mp::Snapshot> parent),
                (override));
};

struct MockSnapshot : public mp::Snapshot
{
    MOCK_METHOD(int, get_index, (), (const, noexcept, override));
    MOCK_METHOD(std::string, get_name, (), (const, override));
    MOCK_METHOD(std::string, get_comment, (), (const, override));
    MOCK_METHOD(QDateTime, get_creation_timestamp, (), (const, noexcept, override));
    MOCK_METHOD(int, get_num_cores, (), (const, noexcept, override));
    MOCK_METHOD(mp::MemorySize, get_mem_size, (), (const, noexcept, override));
    MOCK_METHOD(mp::MemorySize, get_disk_space, (), (const, noexcept, override));
    MOCK_METHOD(mp::VirtualMachine::State, get_state, (), (const, noexcept, override));
    MOCK_METHOD((const std::unordered_map<std::string, mp::VMMount>&), get_mounts, (), (const, noexcept, override));
    MOCK_METHOD(const QJsonObject&, get_metadata, (), (const, noexcept, override));
    MOCK_METHOD(std::shared_ptr<const Snapshot>, get_parent, (), (const, override));
    MOCK_METHOD(std::shared_ptr<Snapshot>, get_parent, (), (override));
    MOCK_METHOD(std::string, get_parents_name, (), (const, override));
    MOCK_METHOD(int, get_parents_index, (), (const, override));
    MOCK_METHOD(void, set_name, (const std::string&), (override));
    MOCK_METHOD(void, set_comment, (const std::string&), (override));
    MOCK_METHOD(void, set_parent, (std::shared_ptr<Snapshot>), (override));
    MOCK_METHOD(void, capture, (), (override));
    MOCK_METHOD(void, erase, (), (override));
    MOCK_METHOD(void, apply, (), (override));
};

struct StubBaseVirtualMachine : public mp::BaseVirtualMachine
{
    StubBaseVirtualMachine(mp::VirtualMachine::State s = mp::VirtualMachine::State::off)
        : StubBaseVirtualMachine{s, std::make_unique<mpt::TempDir>()}
    {
    }

    StubBaseVirtualMachine(mp::VirtualMachine::State s, std::unique_ptr<mpt::TempDir> tmp_dir)
        : mp::BaseVirtualMachine{s, "stub", tmp_dir->path()}, tmp_dir{std::move(tmp_dir)}
    {
    }

    void stop() override
    {
        state = mp::VirtualMachine::State::off;
    }

    void start() override
    {
        state = mp::VirtualMachine::State::running;
    }

    void shutdown() override
    {
        state = mp::VirtualMachine::State::off;
    }

    void suspend() override
    {
        state = mp::VirtualMachine::State::suspended;
    }

    mp::VirtualMachine::State current_state() override
    {
        return state;
    }

    int ssh_port() override
    {
        return 42;
    }

    std::string ssh_hostname(std::chrono::milliseconds /*timeout*/) override
    {
        return "localhost";
    }

    std::string ssh_username() override
    {
        return "ubuntu";
    }

    std::string management_ipv4(const mp::SSHKeyProvider&) override
    {
        return "1.2.3.4";
    }

    std::string ipv6() override
    {
        return "";
    }

    void wait_until_ssh_up(std::chrono::milliseconds /*timeout*/, const mp::SSHKeyProvider& /*key_provider*/) override
    {
    }

    void ensure_vm_is_running() override
    {
    }

    void update_state() override
    {
    }

    void update_cpus(int num_cores) override
    {
    }

    void resize_memory(const mp::MemorySize&) override
    {
    }

    void resize_disk(const mp::MemorySize&) override
    {
    }

    void require_snapshots_support() const override // pretend we support it here
    {
    }

    std::unique_ptr<mpt::TempDir> tmp_dir;
};

struct BaseVM : public Test
{
    void mock_snapshotting()
    {
        EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _))
            .WillRepeatedly(WithArgs<0, 3>([this](const std::string& name, std::shared_ptr<mp::Snapshot> parent) {
                auto ret = std::make_shared<NiceMock<MockSnapshot>>();
                EXPECT_CALL(*ret, get_name).WillRepeatedly(Return(name));
                EXPECT_CALL(*ret, get_index).WillRepeatedly(Return(vm.get_snapshot_count() + 1));
                EXPECT_CALL(*ret, get_parent()).WillRepeatedly(Return(parent));
                EXPECT_CALL(Const(*ret), get_parent()).WillRepeatedly(Return(parent));

                snapshot_album.push_back(ret);

                return ret;
            }));
    }

    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    const mpt::DummyKeyProvider key_provider{"keeper of the seven keys"};
    NiceMock<MockBaseVirtualMachine> vm{"mock-vm"};
    std::vector<std::shared_ptr<MockSnapshot>> snapshot_album;
};

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_opening_a_session)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::running);

    REPLACE(ssh_new, []() { return nullptr; }); // This makes SSH throw when opening a new session.

    auto ip_list = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ip_list.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_executing)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::running);

    // Make SSH throw when trying to execute something.
    mock_ssh_test_fixture.request_exec.returnValue(SSH_ERROR);

    auto ip_list = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ip_list.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_instance_is_off)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::off);

    EXPECT_EQ(base_vm.get_all_ipv4(key_provider).size(), 0u);
}

TEST_F(BaseVM, add_network_interface_throws)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::off);

    MP_EXPECT_THROW_THAT(base_vm.add_network_interface(1, {"eth1", "52:54:00:00:00:00", true}),
                         mp::NotImplementedOnThisBackendException,
                         mpt::match_what(HasSubstr("networks")));
}

struct IpTestParams
{
    int exit_status;
    std::string output;
    std::vector<std::string> expected_ips;
};

struct IpExecution : public BaseVM, public WithParamInterface<IpTestParams>
{
};

TEST_P(IpExecution, get_all_ipv4_works_when_ssh_works)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::running);

    auto test_params = GetParam();
    auto remaining = test_params.output.size();

    ssh_channel_callbacks callbacks{nullptr};
    auto add_channel_cbs = [&callbacks](ssh_channel, ssh_channel_callbacks cb) {
        callbacks = cb;
        return SSH_OK;
    };
    REPLACE(ssh_add_channel_callbacks, add_channel_cbs);

    auto event_dopoll = [&callbacks, &test_params](ssh_event, int timeout) {
        EXPECT_TRUE(callbacks);
        callbacks->channel_exit_status_function(nullptr, nullptr, test_params.exit_status, callbacks->userdata);
        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, event_dopoll);

    auto channel_read = [&test_params, &remaining](ssh_channel, void* dest, uint32_t count, int is_stderr, int) {
        const auto num_to_copy = std::min(count, static_cast<uint32_t>(remaining));
        const auto begin = test_params.output.begin() + test_params.output.size() - remaining;
        std::copy_n(begin, num_to_copy, reinterpret_cast<char*>(dest));
        remaining -= num_to_copy;
        return num_to_copy;
    };
    REPLACE(ssh_channel_read_timeout, channel_read);

    auto ip_list = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ip_list, test_params.expected_ips);
}

INSTANTIATE_TEST_SUITE_P(
    BaseVM, IpExecution,
    Values(IpTestParams{0, "eth0             UP             192.168.2.168/24 \n", {"192.168.2.168"}},
           IpTestParams{0, "eth1             UP             192.168.2.169/24 metric 100 \n", {"192.168.2.169"}},
           IpTestParams{0,
                        "wlp4s0           UP             192.168.2.8/24 \n"
                        "virbr0           DOWN           192.168.3.1/24 \n"
                        "tun0             UNKNOWN        10.172.66.5/18 \n",
                        {"192.168.2.8", "192.168.3.1", "10.172.66.5"}},
           IpTestParams{0, "", {}}));

TEST_F(BaseVM, startsWithNoSnapshots)
{
    EXPECT_EQ(vm.get_num_snapshots(), 0);
}

TEST_F(BaseVM, takesSnapshots)
{
    auto snapshot = std::make_shared<NiceMock<MockSnapshot>>();
    EXPECT_CALL(*snapshot, capture).Times(1);

    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _)).WillOnce(Return(snapshot));
    vm.take_snapshot(mp::VMSpecs{}, "s1", "");

    EXPECT_EQ(vm.get_num_snapshots(), 1);
}

TEST_F(BaseVM, takeSnasphotThrowsIfSpecificSnapshotNotOverridden)
{
    StubBaseVirtualMachine stub{};
    MP_EXPECT_THROW_THAT(stub.take_snapshot({}, "stub-snap", ""),
                         mp::NotImplementedOnThisBackendException,
                         mpt::match_what(HasSubstr("snapshots")));
}

TEST_F(BaseVM, deletesSnapshots)
{
    auto snapshot = std::make_shared<NiceMock<MockSnapshot>>();
    EXPECT_CALL(*snapshot, erase).Times(1);

    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _)).WillOnce(Return(snapshot));
    vm.take_snapshot(mp::VMSpecs{}, "s1", "");
    vm.delete_snapshot("s1");

    EXPECT_EQ(vm.get_num_snapshots(), 0);
}

TEST_F(BaseVM, countsCurrentSnapshots)
{
    const mp::VMSpecs specs{};
    EXPECT_EQ(vm.get_num_snapshots(), 0);

    auto snapshot = std::make_shared<NiceMock<MockSnapshot>>();
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _)).WillRepeatedly(Return(snapshot));

    vm.take_snapshot(specs, "s1", "");
    EXPECT_EQ(vm.get_num_snapshots(), 1);

    vm.take_snapshot(specs, "s2", "");
    vm.take_snapshot(specs, "s3", "");
    EXPECT_EQ(vm.get_num_snapshots(), 3);

    vm.delete_snapshot("s1");
    EXPECT_EQ(vm.get_num_snapshots(), 2);

    vm.delete_snapshot("s2");
    vm.delete_snapshot("s3");
    EXPECT_EQ(vm.get_num_snapshots(), 0);

    vm.take_snapshot(specs, "s4", "");
    EXPECT_EQ(vm.get_num_snapshots(), 1);
}

TEST_F(BaseVM, countsTotalSnapshots)
{
    const mp::VMSpecs specs{};
    EXPECT_EQ(vm.get_num_snapshots(), 0);

    auto snapshot = std::make_shared<NiceMock<MockSnapshot>>();
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _)).WillRepeatedly(Return(snapshot));

    vm.take_snapshot(specs, "s1", "");
    vm.take_snapshot(specs, "s2", "");
    vm.take_snapshot(specs, "s3", "");
    EXPECT_EQ(vm.get_snapshot_count(), 3);

    vm.take_snapshot(specs, "s4", "");
    vm.take_snapshot(specs, "s5", "");
    EXPECT_EQ(vm.get_snapshot_count(), 5);

    vm.delete_snapshot("s1");
    vm.delete_snapshot("s2");
    EXPECT_EQ(vm.get_snapshot_count(), 5);

    vm.delete_snapshot("s4");
    EXPECT_EQ(vm.get_snapshot_count(), 5);

    vm.take_snapshot(specs, "s6", "");
    EXPECT_EQ(vm.get_snapshot_count(), 6);
}

TEST_F(BaseVM, providesSnapshotsView)
{
    mock_snapshotting();
    const mp::VMSpecs specs{};

    auto sname = [](int i) { return fmt::format("s{}", i); };
    for (int i = 1; i < 6; ++i) // +5
        vm.take_snapshot(specs, sname(i), "");
    for (int i = 3; i < 5; ++i) // -2
        vm.delete_snapshot(sname(i));
    for (int i = 6; i < 9; ++i) // +3
        vm.take_snapshot(specs, sname(i), "");
    for (int i : {1, 7}) // -2
        vm.delete_snapshot(sname(i));

    ASSERT_EQ(vm.get_num_snapshots(), 4);
    auto snapshots = vm.view_snapshots();

    EXPECT_THAT(snapshots, SizeIs(4));

    std::vector<int> snapshot_indices{};
    std::transform(snapshots.begin(), snapshots.end(), std::back_inserter(snapshot_indices), [](const auto& snapshot) {
        return snapshot->get_index();
    });

    EXPECT_THAT(snapshot_indices, UnorderedElementsAre(2, 5, 6, 8));
}

TEST_F(BaseVM, providesSnapshotsByIndex)
{
    mock_snapshotting();
    const mp::VMSpecs specs{};

    vm.take_snapshot(specs, "foo", "");
    vm.take_snapshot(specs, "bar", "this and that");
    vm.delete_snapshot("foo");
    vm.take_snapshot(specs, "baz", "this and that");

    for (const auto i : {2, 3})
    {
        EXPECT_THAT(vm.get_snapshot(i), Pointee(Property(&mp::Snapshot::get_index, Eq(i))));
    }
}

TEST_F(BaseVM, providesSnapshotsByName)
{
    mock_snapshotting();

    const mp::VMSpecs specs{};
    const std::string target_name = "pick";
    vm.take_snapshot(specs, "foo", "irrelevant");
    vm.take_snapshot(specs, target_name, "fetch me");
    vm.take_snapshot(specs, "bar", "whatever");
    vm.take_snapshot(specs, "baz", "");
    vm.delete_snapshot("bar");
    vm.take_snapshot(specs, "asdf", "");

    EXPECT_THAT(vm.get_snapshot(target_name), Pointee(Property(&mp::Snapshot::get_name, Eq(target_name))));
}

TEST_F(BaseVM, logsSnapshotHead)
{
    mock_snapshotting();
    const auto name = "asdf";

    auto logger_scope = mpt::MockLogger::inject(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::debug, name);

    vm.take_snapshot({}, name, "");
}

TEST_F(BaseVM, generatesSnapshotNameFromTotalCount)
{
    mock_snapshotting();

    mp::VMSpecs specs{};
    for (int i = 1; i <= 5; ++i)
    {
        vm.take_snapshot(specs, "", "");
        EXPECT_EQ(vm.get_snapshot(i)->get_name(), fmt::format("snapshot{}", i));
    }
}

TEST_F(BaseVM, throwsOnMissingSnapshotByIndex)
{
    mock_snapshotting();

    auto expect_throw = [this](int i) {
        MP_EXPECT_THROW_THAT(vm.get_snapshot(i),
                             std::runtime_error,
                             mpt::match_what(AllOf(HasSubstr(vm.vm_name), HasSubstr(std::to_string(i)))));
    };

    for (int i = -2; i < 4; ++i)
        expect_throw(i);

    const mp::VMSpecs specs{};
    vm.take_snapshot(specs, "foo", "I know kung fu");
    vm.take_snapshot(specs, "bar", "blue pill");
    vm.take_snapshot(specs, "baz", "red pill");

    for (int i : {-2, -1, 0, 4, 5, 100})
        expect_throw(i);
}

TEST_F(BaseVM, throwsOnMissingSnapshotByName)
{
    mock_snapshotting();

    auto expect_throws = [this]() {
        std::array<std::string, 3> missing_names = {"neo", "morpheus", "trinity"};
        for (const auto& name : missing_names)
        {
            MP_EXPECT_THROW_THAT(vm.get_snapshot(name),
                                 mp::NoSuchSnapshotException,
                                 mpt::match_what(AllOf(HasSubstr(vm.vm_name), HasSubstr(name))));
        }
    };

    expect_throws();

    const mp::VMSpecs specs{};
    vm.take_snapshot(specs, "smith", "");
    vm.take_snapshot(specs, "johnson", "");
    vm.take_snapshot(specs, "jones", "");

    expect_throws();
}

TEST_F(BaseVM, throwsOnRepeatedSnapshotName)
{
    mock_snapshotting();

    const mp::VMSpecs specs{};
    auto repeated_given_name = "asdf";
    auto repeated_derived_name = "snapshot3";
    vm.take_snapshot(specs, repeated_given_name, "");
    vm.take_snapshot(specs, repeated_derived_name, "");

    MP_ASSERT_THROW_THAT(vm.take_snapshot(specs, repeated_given_name, ""),
                         mp::SnapshotNameTakenException,
                         mpt::match_what(HasSubstr(repeated_given_name)));
    MP_EXPECT_THROW_THAT(vm.take_snapshot(specs, "", ""), // this would be the third snapshot
                         mp::SnapshotNameTakenException,
                         mpt::match_what(HasSubstr(repeated_derived_name)));
}

TEST_F(BaseVM, snapshotDeletionUpdatesParents)
{
    mock_snapshotting();

    const auto num_snapshots = 3;
    const mp::VMSpecs specs{};
    for (int i = 0; i < num_snapshots; ++i)
        vm.take_snapshot(specs, "", "");

    ASSERT_EQ(snapshot_album.size(), num_snapshots);

    EXPECT_CALL(*snapshot_album[2], set_parent(Eq(snapshot_album[0]))).Times(1);
    vm.delete_snapshot(snapshot_album[1]->get_name());
}

TEST_F(BaseVM, snapshotDeletionThrowsOnMissingSnapshot)
{
    const auto name = "missing";
    MP_EXPECT_THROW_THAT(vm.delete_snapshot(name),
                         mp::NoSuchSnapshotException,
                         mpt::match_what(AllOf(HasSubstr(vm.vm_name), HasSubstr(name))));
}

TEST_F(BaseVM, providesChildrenNames)
{
    mock_snapshotting();

    const auto name_template = "s{}";
    const auto num_snapshots = 5;
    const mp::VMSpecs specs{};
    for (int i = 0; i < num_snapshots; ++i)
        vm.take_snapshot(specs, fmt::format(name_template, i), "");

    ASSERT_EQ(snapshot_album.size(), num_snapshots);

    std::vector<std::string> expected_children_names{};
    for (int i = 1; i < num_snapshots; ++i)
    {
        EXPECT_CALL(Const(*snapshot_album[i]), get_parent()).WillRepeatedly(Return(snapshot_album[0]));
        expected_children_names.push_back(fmt::format(name_template, i));
    }

    EXPECT_THAT(vm.get_childrens_names(snapshot_album[0].get()), UnorderedElementsAreArray(expected_children_names));

    for (int i = 1; i < num_snapshots; ++i)
    {
        EXPECT_THAT(vm.get_childrens_names(snapshot_album[i].get()), IsEmpty());
    }
}

TEST_F(BaseVM, renamesSnapshot)
{
    const std::string old_name = "initial";
    const std::string new_name = "renamed";
    std::string current_name = old_name;

    auto snapshot = std::make_shared<NiceMock<MockSnapshot>>();
    EXPECT_CALL(*snapshot, get_name()).WillRepeatedly(ReturnPointee(&current_name));
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _)).WillOnce(Return(snapshot));

    vm.take_snapshot({}, old_name, "as ;lklkh afa");

    EXPECT_CALL(*snapshot, set_name(Eq(new_name))).WillOnce(Assign(&current_name, new_name));
    vm.rename_snapshot(old_name, new_name);

    EXPECT_EQ(vm.get_snapshot(new_name), snapshot);
}

TEST_F(BaseVM, skipsSnapshotRenamingWithIdenticalName)
{
    mock_snapshotting();

    const auto* name = "fixed";
    vm.take_snapshot({}, name, "not changing");

    ASSERT_EQ(snapshot_album.size(), 1);
    EXPECT_CALL(*snapshot_album[0], set_name).Times(0);

    EXPECT_NO_THROW(vm.rename_snapshot(name, name));
    EXPECT_EQ(vm.get_snapshot(name), snapshot_album[0]);
}

TEST_F(BaseVM, throwsOnRequestToRenameMissingSnapshot)
{
    mock_snapshotting();

    const auto* good_name = "Mafalda";
    const auto* missing_name = "Gui";
    vm.take_snapshot({}, good_name, "");

    ASSERT_EQ(snapshot_album.size(), 1);
    EXPECT_CALL(*snapshot_album[0], set_name).Times(0);

    MP_EXPECT_THROW_THAT(vm.rename_snapshot(missing_name, "Filipe"),
                         mp::NoSuchSnapshotException,
                         mpt::match_what(AllOf(HasSubstr(vm.vm_name), HasSubstr(missing_name))));

    EXPECT_EQ(vm.get_snapshot(good_name), snapshot_album[0]);
}

TEST_F(BaseVM, throwsOnRequestToRenameSnapshotWithRepeatedName)
{
    mock_snapshotting();

    const auto names = std::array{"Mafalda", "Gui"};

    mp::VMSpecs specs{};
    vm.take_snapshot(specs, names[0], "");
    vm.take_snapshot(specs, names[1], "");

    ASSERT_EQ(snapshot_album.size(), 2);
    EXPECT_CALL(*snapshot_album[0], set_name).Times(0);

    MP_EXPECT_THROW_THAT(vm.rename_snapshot(names[0], names[1]),
                         mp::SnapshotNameTakenException,
                         mpt::match_what(AllOf(HasSubstr(vm.vm_name), HasSubstr(names[1]))));
    MP_EXPECT_THROW_THAT(vm.rename_snapshot(names[1], names[0]),
                         mp::SnapshotNameTakenException,
                         mpt::match_what(AllOf(HasSubstr(vm.vm_name), HasSubstr(names[0]))));

    EXPECT_EQ(vm.get_snapshot(names[0]), snapshot_album[0]);
    EXPECT_EQ(vm.get_snapshot(names[1]), snapshot_album[1]);
}

TEST_F(BaseVM, restoresSnapshots)
{
    mock_snapshotting();

    mp::VMMount mount{"src", {}, {}, mp::VMMount::MountType::Classic};

    QJsonObject metadata{};
    metadata["meta"] = "data";

    const mp::VMSpecs original_specs{2,
                                     mp::MemorySize{"3.5G"},
                                     mp::MemorySize{"15G"},
                                     "12:12:12:12:12:12",
                                     {},
                                     "user",
                                     mp::VirtualMachine::State::off,
                                     {{"dst", mount}},
                                     false,
                                     metadata,
                                     {}};

    const auto* snapshot_name = "shoot";
    vm.take_snapshot(original_specs, snapshot_name, "");

    ASSERT_EQ(snapshot_album.size(), 1);
    auto& snapshot = *snapshot_album[0];

    mp::VMSpecs changed_specs = original_specs;
    changed_specs.num_cores = 3;
    changed_specs.mem_size = mp::MemorySize{"5G"};
    changed_specs.disk_space = mp::MemorySize{"35G"};
    changed_specs.state = mp::VirtualMachine::State::stopped;
    changed_specs.mounts.clear();
    changed_specs.metadata["data"] = "meta";
    changed_specs.metadata["meta"] = "toto";

    EXPECT_CALL(snapshot, apply);
    EXPECT_CALL(snapshot, get_state).WillRepeatedly(Return(original_specs.state));
    EXPECT_CALL(snapshot, get_num_cores).WillRepeatedly(Return(original_specs.num_cores));
    EXPECT_CALL(snapshot, get_mem_size).WillRepeatedly(Return(original_specs.mem_size));
    EXPECT_CALL(snapshot, get_disk_space).WillRepeatedly(Return(original_specs.disk_space));
    EXPECT_CALL(snapshot, get_mounts).WillRepeatedly(ReturnRef(original_specs.mounts));
    EXPECT_CALL(snapshot, get_metadata).WillRepeatedly(ReturnRef(original_specs.metadata));

    vm.restore_snapshot(snapshot_name, changed_specs);

    EXPECT_EQ(original_specs, changed_specs);
}

TEST_F(BaseVM, usesRestoredSnapshotAsParentForNewSnapshots)
{
    mock_snapshotting();

    mp::VMSpecs specs{};
    const std::string root_name{"first"};
    vm.take_snapshot(specs, root_name, "");
    auto root_snapshot = snapshot_album[0];

    ASSERT_EQ(snapshot_album.size(), 1);
    EXPECT_EQ(vm.take_snapshot(specs, "second", "")->get_parent().get(), root_snapshot.get());
    ASSERT_EQ(snapshot_album.size(), 2);
    EXPECT_EQ(vm.take_snapshot(specs, "third", "")->get_parent().get(), snapshot_album[1].get());

    std::unordered_map<std::string, mp::VMMount> mounts;
    EXPECT_CALL(*root_snapshot, get_mounts).WillRepeatedly(ReturnRef(mounts));

    QJsonObject metadata{};
    EXPECT_CALL(*root_snapshot, get_metadata).WillRepeatedly(ReturnRef(metadata));

    vm.restore_snapshot(root_name, specs);
    EXPECT_EQ(vm.take_snapshot(specs, "fourth", "")->get_parent().get(), root_snapshot.get());
}

TEST_F(BaseVM, loadSnasphotThrowsIfSnapshotsNotImplemented)
{
    StubBaseVirtualMachine stub{};
    mpt::make_file_with_content(stub.tmp_dir->filePath("0001.snapshot.json"), "whatever-content");
    MP_EXPECT_THROW_THAT(stub.load_snapshots(),
                         mp::NotImplementedOnThisBackendException,
                         mpt::match_what(HasSubstr("snapshots")));
}

using SpacePadding = std::tuple<std::string, std::string>;
struct TestLoadingOfPaddedGenericSnapshotInfo : public BaseVM, WithParamInterface<SpacePadding>
{
    void SetUp() override
    {
        static const auto space_matcher = MatchesRegex("\\s*");
        ASSERT_THAT(padding_left, space_matcher);
        ASSERT_THAT(padding_right, space_matcher);
    }

    const std::string& padding_left = std::get<0>(GetParam());
    const std::string& padding_right = std::get<1>(GetParam());
};

TEST_P(TestLoadingOfPaddedGenericSnapshotInfo, loadsAndUsesTotalSnapshotCount)
{
    mock_snapshotting();

    int initial_count = 42;
    auto count_text = fmt::format("{}{}{}", padding_left, initial_count, padding_right);
    mpt::make_file_with_content(vm.tmp_dir->filePath("snapshot-count"), count_text);

    EXPECT_NO_THROW(vm.load_snapshots());

    mp::VMSpecs specs{};
    for (int i = 1; i <= 5; ++i)
    {
        int expected_idx = initial_count + i;
        vm.take_snapshot(specs, "", "");
        EXPECT_EQ(vm.get_snapshot(expected_idx)->get_name(), fmt::format("snapshot{}", expected_idx));
    }
}

TEST_P(TestLoadingOfPaddedGenericSnapshotInfo, loadsAndUsesSnapshotHeadIndex)
{
    mock_snapshotting();

    int head_index = 13;
    auto snapshot = std::make_shared<NiceMock<MockSnapshot>>();
    EXPECT_CALL(vm, get_snapshot(head_index)).WillOnce(Return(snapshot));

    auto head_text = fmt::format("{}{}{}", padding_left, head_index, padding_right);
    mpt::make_file_with_content(vm.tmp_dir->filePath("snapshot-head"), head_text);
    mpt::make_file_with_content(vm.tmp_dir->filePath("snapshot-count"), "31");

    EXPECT_NO_THROW(vm.load_snapshots());

    auto name = "julius";
    vm.take_snapshot({}, name, "");
    EXPECT_EQ(vm.get_snapshot(name)->get_parent().get(), snapshot.get());
}

std::vector<std::string> space_paddings = {"", " ", "    ", "\n", " \n", "\n\n\n", "\t", "\t\t\t", "\t \n  \t   "};
INSTANTIATE_TEST_SUITE_P(BaseVM,
                         TestLoadingOfPaddedGenericSnapshotInfo,
                         Combine(ValuesIn(space_paddings), ValuesIn(space_paddings)));

TEST_F(BaseVM, loadsSnasphots)
{
    static constexpr auto num_snapshots = 5;
    static constexpr auto name_prefix = "blankpage";
    static constexpr auto generate_snapshot_name = [](int count) { return fmt::format("{}{}", name_prefix, count); };
    static constexpr auto index_digits_regex =
#ifdef MULTIPASS_PLATFORM_WINDOWS
        R"(\d\d\d\d)";
#else
        "[[:digit:]]{4}";
#endif
    static const auto file_regex = fmt::format(R"(.*{}\.snapshot\.json)", index_digits_regex);

    auto& expectation = EXPECT_CALL(vm, make_specific_snapshot(mpt::match_qstring(MatchesRegex(file_regex))));

    using NiceMockSnapshot = NiceMock<MockSnapshot>;
    std::array<std::shared_ptr<NiceMockSnapshot>, num_snapshots> snapshot_bag{};
    generate(snapshot_bag.begin(), snapshot_bag.end(), [this, &expectation] {
        static int idx = 1;

        const auto path = vm.tmp_dir->filePath(QString::fromStdString(fmt::format("{:04}.snapshot.json", idx)));
        mpt::make_file_with_content(path, "stub");

        auto ret = std::make_shared<NiceMockSnapshot>();
        EXPECT_CALL(*ret, get_index).WillRepeatedly(Return(idx));
        EXPECT_CALL(*ret, get_name).WillRepeatedly(Return(generate_snapshot_name(idx++)));
        expectation.WillOnce(Return(ret));

        return ret;
    });

    mpt::make_file_with_content(vm.tmp_dir->filePath("snapshot-head"), fmt::format("{}", num_snapshots));
    mpt::make_file_with_content(vm.tmp_dir->filePath("snapshot-count"), fmt::format("{}", num_snapshots));

    EXPECT_NO_THROW(vm.load_snapshots());

    for (int i = 0; i < num_snapshots; ++i)
    {
        const auto idx = i + 1;
        EXPECT_EQ(vm.get_snapshot(idx)->get_name(), generate_snapshot_name(idx));
    }
}

TEST_F(BaseVM, throwsIfThereAreSnapshotsToLoadButNoGenericInfo)
{
    auto snapshot = std::make_shared<NiceMock<MockSnapshot>>();

    const auto name = "snapshot1";
    EXPECT_CALL(*snapshot, get_name).WillRepeatedly(Return(name));
    EXPECT_CALL(*snapshot, get_index).WillRepeatedly(Return(1));
    EXPECT_CALL(vm, make_specific_snapshot(_)).Times(2).WillRepeatedly(Return(snapshot));

    mpt::make_file_with_content(vm.tmp_dir->filePath("0001.snapshot.json"), "stub");
    MP_EXPECT_THROW_THAT(vm.load_snapshots(),
                         mp::FileOpenFailedException,
                         mpt::match_what(HasSubstr("snapshot-count")));

    vm.delete_snapshot(name);
    mpt::make_file_with_content(vm.tmp_dir->filePath("snapshot-count"), "1");
    MP_EXPECT_THROW_THAT(vm.load_snapshots(), mp::FileOpenFailedException, mpt::match_what(HasSubstr("snapshot-head")));
}

} // namespace

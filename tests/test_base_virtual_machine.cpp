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
#include "mock_cloud_init_file_ops.h"
#include "mock_logger.h"
#include "mock_snapshot.h"
#include "mock_ssh_test_fixture.h"
#include "mock_utils.h"
#include "mock_virtual_machine.h"
#include "temp_dir.h"

#include <shared/base_virtual_machine.h>

#include <multipass/exceptions/file_open_failed_exception.h>
#include <multipass/exceptions/internal_timeout_exception.h>
#include <multipass/exceptions/ip_unavailable_exception.h>
#include <multipass/exceptions/snapshot_exceptions.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/logging/level.h>
#include <multipass/snapshot.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/vm_specs.h>

#include <algorithm>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;
using St = mp::VirtualMachine::State;

namespace
{
struct MockBaseVirtualMachine : public mpt::MockVirtualMachineT<mp::BaseVirtualMachine>
{
    template <typename... Args>
    MockBaseVirtualMachine(Args&&... args)
        : mpt::MockVirtualMachineT<mp::BaseVirtualMachine>{std::forward<Args>(args)...}
    {
        const auto& const_self = *this;
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, get_all_ipv4, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, view_snapshots, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, get_num_snapshots, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, take_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, rename_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, delete_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, restore_snapshot, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, load_snapshots, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, get_childrens_names, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, get_snapshot_count, mp::BaseVirtualMachine);
        MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(*this, get_snapshot, mp::BaseVirtualMachine, (An<int>()));
        MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(const_self, get_snapshot, mp::BaseVirtualMachine, (An<int>()));
        MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(*this,
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
                 const std::string& instance_id,
                 const mp::VMSpecs& specs,
                 std::shared_ptr<mp::Snapshot> parent),
                (override));

    using mp::BaseVirtualMachine::renew_ssh_session; // promote to public

    void simulate_state(St state)
    {
        this->state = state;
        ON_CALL(*this, current_state).WillByDefault(Return(state));
    }

    void simulate_ssh_exec() // use if premocking libssh stuff
    {
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, ssh_exec, mp::BaseVirtualMachine);
    }

    void simulate_waiting_for_ssh() // use if premocking libssh stuff
    {
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, wait_until_ssh_up, mp::BaseVirtualMachine);
    }

    void simulate_cloud_init()
    {
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, wait_for_cloud_init, mp::BaseVirtualMachine);
    }

    void simulate_no_snapshots_support() const // doing this here to access protected method on the base
    {
        MP_DELEGATE_MOCK_CALLS_ON_BASE(*this, require_snapshots_support, mp::BaseVirtualMachine);
    }
};

struct StubBaseVirtualMachine : public mp::BaseVirtualMachine
{
    StubBaseVirtualMachine(St s = St::off) : StubBaseVirtualMachine{s, std::make_unique<mpt::TempDir>()}
    {
    }

    StubBaseVirtualMachine(St s, std::unique_ptr<mpt::TempDir> tmp_dir)
        : mp::BaseVirtualMachine{s, "stub", mpt::StubSSHKeyProvider{}, tmp_dir->path()}, tmp_dir{std::move(tmp_dir)}
    {
    }

    void start() override
    {
        state = St::running;
    }

    void shutdown(ShutdownPolicy shutdown_policy = ShutdownPolicy::Powerdown) override
    {
        state = St::off;
    }

    void suspend() override
    {
        state = St::suspended;
    }

    St current_state() override
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

    std::string management_ipv4() override
    {
        return "1.2.3.4";
    }

    std::string ipv6() override
    {
        return "";
    }

    void wait_until_ssh_up(std::chrono::milliseconds) override
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
        EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _))
            .WillRepeatedly(WithArgs<0, 4>([this](const std::string& name, std::shared_ptr<mp::Snapshot> parent) {
                auto ret = std::make_shared<NiceMock<mpt::MockSnapshot>>();
                EXPECT_CALL(*ret, get_name).WillRepeatedly(Return(name));
                EXPECT_CALL(*ret, get_index).WillRepeatedly(Return(vm.get_snapshot_count() + 1));
                EXPECT_CALL(*ret, get_parent()).WillRepeatedly(Return(parent));
                EXPECT_CALL(Const(*ret), get_parent()).WillRepeatedly(Return(parent));
                EXPECT_CALL(*ret, get_parents_index).WillRepeatedly(Return(parent ? parent->get_index() : 0));

                snapshot_album.push_back(ret);

                return ret;
            }));
    }

    QString get_snapshot_file_path(int idx) const
    {
        assert(idx > 0 && "need positive index");

        return vm.tmp_dir->filePath(QString::fromStdString(fmt::format("{:04}.snapshot.json", idx)));
    }

    static std::string n_occurrences(const std::string& regex, int n)
    {
        assert(n > 0 && "need positive n");
        return
#ifdef MULTIPASS_PLATFORM_WINDOWS
            fmt::to_string(fmt::join(std::vector(n, regex), ""));
#else
            fmt::format("{}{{{}}}", regex, n);
#endif
    }

    static auto make_index_file_contents_matcher(int idx)
    {
        assert(idx > 0 && "need positive index");

        return MatchesRegex(fmt::format("{0}*{1}{0}*", space_char_class, idx));
    }

    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    const mpt::DummyKeyProvider key_provider{"keeper of the seven keys"};
    NiceMock<MockBaseVirtualMachine> vm{"mock-vm", key_provider};
    std::vector<std::shared_ptr<mpt::MockSnapshot>> snapshot_album;
    QString head_path = vm.tmp_dir->filePath(head_filename);
    QString count_path = vm.tmp_dir->filePath(count_filename);
    const mpt::MockCloudInitFileOps::GuardedMock mock_cloud_init_file_ops_injection =
        mpt::MockCloudInitFileOps::inject<NiceMock>();
    static constexpr bool on_windows =
#ifdef MULTIPASS_PLATFORM_WINDOWS
        true;
#else
        false;
#endif
    static constexpr auto* head_filename = "snapshot-head";
    static constexpr auto* count_filename = "snapshot-count";
    static constexpr auto space_char_class = on_windows ? "\\s" : "[[:space:]]";
    static constexpr auto digit_char_class = on_windows ? "\\d" : "[[:digit:]]";
};

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_opening_a_session)
{
    vm.simulate_state(St::running);
    vm.simulate_ssh_exec();
    REPLACE(ssh_new, []() { return nullptr; }); // This makes SSH throw when opening a new session.

    auto ip_list = vm.get_all_ipv4();
    EXPECT_EQ(ip_list.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_executing)
{
    vm.simulate_state(St::running);
    vm.simulate_ssh_exec();

    // Make SSH throw when trying to execute something.
    mock_ssh_test_fixture.request_exec.returnValue(SSH_ERROR);

    auto ip_list = vm.get_all_ipv4();
    EXPECT_EQ(ip_list.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_instance_is_off)
{
    vm.simulate_state(St::off);

    EXPECT_EQ(vm.get_all_ipv4().size(), 0u);
}

TEST_F(BaseVM, add_network_interface_throws)
{
    StubBaseVirtualMachine base_vm(St::off);

    MP_EXPECT_THROW_THAT(base_vm.add_network_interface(1, "", {"eth1", "52:54:00:00:00:00", true}),
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
    vm.simulate_state(St::running);
    vm.simulate_ssh_exec();

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

    auto ip_list = vm.get_all_ipv4();
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

TEST_F(BaseVM, throwsOnSnapshotsRequestIfNotSupported)
{
    vm.simulate_no_snapshots_support();
    MP_EXPECT_THROW_THAT(vm.get_num_snapshots(),
                         mp::NotImplementedOnThisBackendException,
                         mpt::match_what(HasSubstr("snapshots")));
}

TEST_F(BaseVM, takesSnapshots)
{
    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, capture).Times(1);

    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillOnce(Return(snapshot));
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
    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, erase).Times(1);

    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillOnce(Return(snapshot));
    vm.take_snapshot(mp::VMSpecs{}, "s1", "");
    vm.delete_snapshot("s1");

    EXPECT_EQ(vm.get_num_snapshots(), 0);
}

TEST_F(BaseVM, countsCurrentSnapshots)
{
    const mp::VMSpecs specs{};
    EXPECT_EQ(vm.get_num_snapshots(), 0);

    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillRepeatedly(Return(snapshot));

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

    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillRepeatedly(Return(snapshot));

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

    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, get_name()).WillRepeatedly(ReturnPointee(&current_name));
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillOnce(Return(snapshot));

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
                                     St::off,
                                     {{"dst", mount}},
                                     false,
                                     metadata};

    const auto* snapshot_name = "shoot";
    vm.take_snapshot(original_specs, snapshot_name, "");

    ASSERT_EQ(snapshot_album.size(), 1);
    auto& snapshot = *snapshot_album[0];

    mp::VMSpecs changed_specs = original_specs;
    changed_specs.num_cores = 3;
    changed_specs.mem_size = mp::MemorySize{"5G"};
    changed_specs.disk_space = mp::MemorySize{"35G"};
    changed_specs.state = St::stopped;
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

TEST_F(BaseVM, restoresSnapshotsWithExtraInterfaceDiff)
{
    mock_snapshotting();

    // default value of VMSpecs::state is off, so restore_snapshot will pass the assert_vm_stopped check, the other
    // fields do not matter, and VMSpecs::extra_interfaces is defaulted to be empty, which is we want.
    const mp::VMSpecs original_specs{};
    const auto* snapshot_name = "snapshot1";
    vm.take_snapshot(original_specs, snapshot_name, "");

    ASSERT_EQ(snapshot_album.size(), 1);
    const auto& snapshot = *snapshot_album[0];

    mp::VMSpecs new_specs = original_specs;
    new_specs.extra_interfaces =
        std::vector<mp::NetworkInterface>{{"id", "52:54:00:56:78:91", true}, {"id", "52:54:00:56:78:92", true}};

    // the ref return functions can not use the default mock behavior, so they need to be specified
    EXPECT_CALL(snapshot, get_mounts).WillOnce(ReturnRef(original_specs.mounts));
    EXPECT_CALL(snapshot, get_metadata).WillOnce(ReturnRef(original_specs.metadata));

    // set the behavior of get_extra_interfaces to cause the difference to the new space extra interfaces
    EXPECT_CALL(snapshot, get_extra_interfaces).Times(3).WillRepeatedly(Return(original_specs.extra_interfaces));

    EXPECT_CALL(*mock_cloud_init_file_ops_injection.first,
                update_cloud_init_with_new_extra_interfaces_and_new_id(_, _, _, _))
        .Times(1);

    vm.restore_snapshot(snapshot_name, new_specs);
    EXPECT_EQ(original_specs, new_specs);
}

TEST_F(BaseVM, usesRestoredSnapshotAsParentForNewSnapshots)
{
    mock_snapshotting();

    mp::VMSpecs specs{};
    const std::string root_name{"first"};
    vm.take_snapshot(specs, root_name, "");
    auto root_snapshot = snapshot_album[0];

    ASSERT_EQ(snapshot_album.size(), 1);
    EXPECT_EQ(vm.take_snapshot(specs, "second", "")->get_parent(), root_snapshot);
    ASSERT_EQ(snapshot_album.size(), 2);
    EXPECT_EQ(vm.take_snapshot(specs, "third", "")->get_parent().get(), snapshot_album[1].get());

    std::unordered_map<std::string, mp::VMMount> mounts;
    EXPECT_CALL(*root_snapshot, get_mounts).WillRepeatedly(ReturnRef(mounts));

    QJsonObject metadata{};
    EXPECT_CALL(*root_snapshot, get_metadata).WillRepeatedly(ReturnRef(metadata));

    vm.restore_snapshot(root_name, specs);
    EXPECT_EQ(vm.take_snapshot(specs, "fourth", "")->get_parent(), root_snapshot);
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
        static const auto space_matcher = MatchesRegex(fmt::format("{}*", space_char_class));
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
    mpt::make_file_with_content(count_path, count_text);

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
    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(vm, get_snapshot(head_index)).WillOnce(Return(snapshot));

    auto head_text = fmt::format("{}{}{}", padding_left, head_index, padding_right);
    mpt::make_file_with_content(head_path, head_text);
    mpt::make_file_with_content(count_path, "31");

    EXPECT_NO_THROW(vm.load_snapshots());

    auto name = "julius";
    vm.take_snapshot({}, name, "");
    EXPECT_EQ(vm.get_snapshot(name)->get_parent(), snapshot);
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
    static const auto index_digits_regex = n_occurrences(digit_char_class, 4);
    static const auto file_regex = fmt::format(R"(.*{}\.snapshot\.json)", index_digits_regex);

    auto& expectation = EXPECT_CALL(vm, make_specific_snapshot(mpt::match_qstring(MatchesRegex(file_regex))));

    using NiceMockSnapshot = NiceMock<mpt::MockSnapshot>;
    std::array<std::shared_ptr<NiceMockSnapshot>, num_snapshots> snapshot_bag{};
    generate(snapshot_bag.begin(), snapshot_bag.end(), [this, &expectation] {
        static int idx = 1;

        mpt::make_file_with_content(get_snapshot_file_path(idx), "stub");

        auto ret = std::make_shared<NiceMockSnapshot>();
        EXPECT_CALL(*ret, get_index).WillRepeatedly(Return(idx));
        EXPECT_CALL(*ret, get_name).WillRepeatedly(Return(generate_snapshot_name(idx++)));
        expectation.WillOnce(Return(ret));

        return ret;
    });

    mpt::make_file_with_content(head_path, fmt::format("{}", num_snapshots));
    mpt::make_file_with_content(count_path, fmt::format("{}", num_snapshots));

    EXPECT_NO_THROW(vm.load_snapshots());

    for (int i = 0; i < num_snapshots; ++i)
    {
        const auto idx = i + 1;
        EXPECT_EQ(vm.get_snapshot(idx)->get_name(), generate_snapshot_name(idx));
    }
}

TEST_F(BaseVM, throwsIfThereAreSnapshotsToLoadButNoGenericInfo)
{
    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();

    const auto name = "snapshot1";
    EXPECT_CALL(*snapshot, get_name).WillRepeatedly(Return(name));
    EXPECT_CALL(*snapshot, get_index).WillRepeatedly(Return(1));
    EXPECT_CALL(vm, make_specific_snapshot(_)).Times(2).WillRepeatedly(Return(snapshot));

    mpt::make_file_with_content(get_snapshot_file_path(1), "stub");
    MP_EXPECT_THROW_THAT(vm.load_snapshots(), mp::FileOpenFailedException, mpt::match_what(HasSubstr(count_filename)));

    vm.delete_snapshot(name);
    mpt::make_file_with_content(count_path, "1");
    MP_EXPECT_THROW_THAT(vm.load_snapshots(), mp::FileOpenFailedException, mpt::match_what(HasSubstr(head_filename)));
}

TEST_F(BaseVM, throwsIfLoadedSnapshotsNameIsTaken)
{
    const auto common_name = "common";
    auto snapshot1 = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    auto snapshot2 = std::make_shared<NiceMock<mpt::MockSnapshot>>();

    EXPECT_CALL(*snapshot1, get_name).WillRepeatedly(Return(common_name));
    EXPECT_CALL(*snapshot1, get_index).WillRepeatedly(Return(1));

    EXPECT_CALL(*snapshot2, get_name).WillRepeatedly(Return(common_name));
    EXPECT_CALL(*snapshot2, get_index).WillRepeatedly(Return(2));

    EXPECT_CALL(vm, make_specific_snapshot(_)).WillOnce(Return(snapshot1)).WillOnce(Return(snapshot2));

    mpt::make_file_with_content(get_snapshot_file_path(1), "stub");
    mpt::make_file_with_content(get_snapshot_file_path(2), "stub");
    mpt::make_file_with_content(head_path, "1");
    mpt::make_file_with_content(count_path, "2");

    MP_EXPECT_THROW_THAT(vm.load_snapshots(), mp::SnapshotNameTakenException, mpt::match_what(HasSubstr(common_name)));
}

TEST_F(BaseVM, snapshotDeletionRestoresParentsOnFailure)
{
    mock_snapshotting();

    const auto num_snapshots = 3;
    const mp::VMSpecs specs{};
    for (int i = 0; i < num_snapshots; ++i)
        vm.take_snapshot(specs, "", "");

    ASSERT_EQ(snapshot_album.size(), num_snapshots);

    EXPECT_CALL(*snapshot_album[2], set_parent(Eq(snapshot_album[0]))).Times(1);
    EXPECT_CALL(*snapshot_album[2], set_parent(Eq(snapshot_album[1]))).Times(1); // rollback

    EXPECT_CALL(*snapshot_album[1], erase).WillOnce(Throw(std::runtime_error{"intentional"}));
    EXPECT_ANY_THROW(vm.delete_snapshot(snapshot_album[1]->get_name()));
}

TEST_F(BaseVM, snapshotDeletionKeepsHeadOnFailure)
{
    mock_snapshotting();

    mp::VMSpecs specs{};
    vm.take_snapshot(specs, "", "");
    vm.take_snapshot(specs, "", "");

    ASSERT_EQ(snapshot_album.size(), 2);

    EXPECT_CALL(*snapshot_album[1], erase).WillOnce(Throw(std::runtime_error{"intentional"}));
    EXPECT_ANY_THROW(vm.delete_snapshot(snapshot_album[1]->get_name()));

    EXPECT_EQ(vm.take_snapshot(specs, "", "")->get_parent().get(), snapshot_album[1].get());
}

TEST_F(BaseVM, takeSnapshotRevertsToNullHeadOnFirstFailure)
{
    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, capture).WillOnce(Throw(std::runtime_error{"intentional"}));
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillOnce(Return(snapshot)).RetiresOnSaturation();

    mp::VMSpecs specs{};
    EXPECT_ANY_THROW(vm.take_snapshot(specs, "", ""));

    mock_snapshotting();
    EXPECT_EQ(vm.take_snapshot(specs, "", "")->get_parent().get(), nullptr);
}

TEST_F(BaseVM, takeSnapshotRevertsHeadAndCount)
{
    auto early_snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*early_snapshot, get_name).WillRepeatedly(Return("asdf"));
    EXPECT_CALL(*early_snapshot, get_index).WillRepeatedly(Return(1));

    EXPECT_CALL(vm, make_specific_snapshot(_)).WillOnce(Return(early_snapshot));

    mpt::make_file_with_content(get_snapshot_file_path(1), "stub");
    mpt::make_file_with_content(head_path, "1");
    mpt::make_file_with_content(count_path, "1");

    vm.load_snapshots();

    constexpr auto attempted_name = "fdsa";
    auto failing_snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();

    EXPECT_CALL(*failing_snapshot, get_name).WillRepeatedly(Return(attempted_name));
    EXPECT_CALL(*failing_snapshot, get_index).WillRepeatedly(Return(2));
    EXPECT_CALL(*failing_snapshot, get_parents_index)
        .WillOnce(Throw(std::runtime_error{"intentional"})) // causes persisting to break, after successful capture
        .RetiresOnSaturation();

    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillOnce(Return(failing_snapshot)).RetiresOnSaturation();

    mp::VMSpecs specs{};
    EXPECT_ANY_THROW(vm.take_snapshot(specs, attempted_name, ""));

    mock_snapshotting();
    auto new_snapshot = vm.take_snapshot(specs, attempted_name, "");
    EXPECT_EQ(new_snapshot->get_parent(), early_snapshot);
    EXPECT_EQ(new_snapshot->get_index(), 2); // snapshot count not increased by failed snapshot
}

TEST_F(BaseVM, renameFailureIsReverted)
{
    std::string current_name = "before";
    std::string attempted_name = "after";
    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, get_name()).WillRepeatedly(Return(current_name));
    EXPECT_CALL(vm, make_specific_snapshot(_, _, _, _, _)).WillOnce(Return(snapshot));

    vm.take_snapshot({}, current_name, "");

    EXPECT_CALL(*snapshot, set_name(Eq(attempted_name))).WillOnce(Throw(std::runtime_error{"intentional"}));
    EXPECT_ANY_THROW(vm.rename_snapshot(current_name, attempted_name));

    EXPECT_EQ(vm.get_snapshot(current_name), snapshot);
}

TEST_F(BaseVM, persistsGenericSnapshotInfoWhenTakingSnapshot)
{
    mock_snapshotting();

    ASSERT_EQ(vm.get_snapshot_count(), 0);

    ASSERT_FALSE(QFileInfo{head_path}.exists());
    ASSERT_FALSE(QFileInfo{count_path}.exists());

    mp::VMSpecs specs{};
    for (int i = 1; i < 5; ++i)
    {
        vm.take_snapshot(specs, "", "");
        ASSERT_TRUE(QFileInfo{head_path}.exists());
        ASSERT_TRUE(QFileInfo{count_path}.exists());

        auto regex_matcher = make_index_file_contents_matcher(i);
        EXPECT_THAT(mpt::load(head_path).toStdString(), regex_matcher);
        EXPECT_THAT(mpt::load(count_path).toStdString(), regex_matcher);
    }
}

TEST_F(BaseVM, removesGenericSnapshotInfoFilesOnFirstFailure)
{
    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    auto& mock_utils = *mock_utils_ptr;
    mock_snapshotting();

    ASSERT_FALSE(QFileInfo{head_path}.exists());
    ASSERT_FALSE(QFileInfo{count_path}.exists());

    MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(mock_utils,
                                                 make_file_with_content,
                                                 mp::Utils,
                                                 (EndsWith(head_filename), _, Eq(true)));
    EXPECT_CALL(mock_utils, make_file_with_content(EndsWith(head_filename), _, Eq(true)));
    EXPECT_CALL(mock_utils, make_file_with_content(EndsWith(count_filename), _, Eq(true)))
        .WillOnce(Throw(std::runtime_error{"intentional"}));

    EXPECT_ANY_THROW(vm.take_snapshot({}, "", ""));

    EXPECT_FALSE(QFileInfo{head_path}.exists());
    EXPECT_FALSE(QFileInfo{count_path}.exists());
}

TEST_F(BaseVM, restoresGenericSnapshotInfoFileContents)
{
    mock_snapshotting();

    mp::VMSpecs specs{};
    vm.take_snapshot(specs, "", "");

    ASSERT_TRUE(QFileInfo{head_path}.exists());
    ASSERT_TRUE(QFileInfo{count_path}.exists());

    auto regex_matcher = make_index_file_contents_matcher(1);
    EXPECT_THAT(mpt::load(head_path).toStdString(), regex_matcher);
    EXPECT_THAT(mpt::load(count_path).toStdString(), regex_matcher);

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject<NiceMock>();
    auto& mock_utils = *mock_utils_ptr;

    MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(mock_utils, make_file_with_content, mp::Utils, (_, _, Eq(true)));
    EXPECT_CALL(mock_utils, make_file_with_content(EndsWith(head_filename), _, Eq(true))).Times(2);
    EXPECT_CALL(mock_utils, make_file_with_content(EndsWith(count_filename), _, Eq(true)))
        .WillOnce(Throw(std::runtime_error{"intentional"}))
        .WillOnce(DoDefault());

    EXPECT_ANY_THROW(vm.take_snapshot({}, "", ""));

    EXPECT_TRUE(QFileInfo{head_path}.exists());
    EXPECT_TRUE(QFileInfo{count_path}.exists());
    EXPECT_THAT(mpt::load(head_path).toStdString(), regex_matcher);
    EXPECT_THAT(mpt::load(count_path).toStdString(), regex_matcher);
}

TEST_F(BaseVM, persistsHeadIndexOnRestore)
{
    mock_snapshotting();

    mp::VMSpecs specs{};
    const auto intended_snapshot = "this-one";
    vm.take_snapshot(specs, "foo", "");
    vm.take_snapshot(specs, intended_snapshot, "");
    vm.take_snapshot(specs, "bar", "");

    std::unordered_map<std::string, mp::VMMount> mounts;
    EXPECT_CALL(*snapshot_album[1], get_mounts).WillRepeatedly(ReturnRef(mounts));

    QJsonObject metadata{};
    EXPECT_CALL(*snapshot_album[1], get_metadata).WillRepeatedly(ReturnRef(metadata));

    vm.restore_snapshot(intended_snapshot, specs);
    EXPECT_TRUE(QFileInfo{head_path}.exists());

    auto regex_matcher = make_index_file_contents_matcher(snapshot_album[1]->get_index());
    EXPECT_THAT(mpt::load(head_path).toStdString(), regex_matcher);
}

TEST_F(BaseVM, rollsbackFailedRestore)
{
    mock_snapshotting();

    const mp::VMSpecs original_specs{1,
                                     mp::MemorySize{"1.5G"},
                                     mp::MemorySize{"4G"},
                                     "ab:ab:ab:ab:ab:ab",
                                     {},
                                     "me",
                                     St::off,
                                     {},
                                     false,
                                     {}};

    vm.take_snapshot(original_specs, "", "");

    auto target_snapshot_name = "this one";
    vm.take_snapshot(original_specs, target_snapshot_name, "");
    vm.take_snapshot(original_specs, "", "");

    ASSERT_EQ(snapshot_album.size(), 3);
    auto& target_snapshot = *snapshot_album[1];
    auto& last_snapshot = *snapshot_album[2];

    mp::VMMount mount{"src", {}, {}, mp::VMMount::MountType::Classic};

    auto changed_specs = original_specs;
    changed_specs.num_cores = 4;
    changed_specs.mem_size = mp::MemorySize{"2G"};
    changed_specs.state = multipass::VirtualMachine::State::running;
    changed_specs.mounts["dst"] = mount;
    changed_specs.metadata["blah"] = "this and that";

    EXPECT_CALL(target_snapshot, get_state).WillRepeatedly(Return(original_specs.state));
    EXPECT_CALL(target_snapshot, get_num_cores).WillRepeatedly(Return(original_specs.num_cores));
    EXPECT_CALL(target_snapshot, get_mem_size).WillRepeatedly(Return(original_specs.mem_size));
    EXPECT_CALL(target_snapshot, get_disk_space).WillRepeatedly(Return(original_specs.disk_space));
    EXPECT_CALL(target_snapshot, get_mounts).WillRepeatedly(ReturnRef(original_specs.mounts));
    EXPECT_CALL(target_snapshot, get_metadata).WillRepeatedly(ReturnRef(original_specs.metadata));

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, make_file_with_content(_, _, _))
        .WillOnce(Throw(std::runtime_error{"intentional"}))
        .WillRepeatedly(DoDefault());

    auto current_specs = changed_specs;
    EXPECT_ANY_THROW(vm.restore_snapshot(target_snapshot_name, current_specs));
    EXPECT_EQ(changed_specs, current_specs);

    auto regex_matcher = make_index_file_contents_matcher(last_snapshot.get_index());
    EXPECT_THAT(mpt::load(head_path).toStdString(), regex_matcher);

    EXPECT_EQ(vm.take_snapshot(current_specs, "", "")->get_parent().get(), &last_snapshot);
}

TEST_F(BaseVM, waitForCloudInitNoErrorsAndDoneDoesNotThrow)
{
    vm.simulate_cloud_init();
    EXPECT_CALL(vm, ensure_vm_is_running()).WillRepeatedly(Return());
    EXPECT_CALL(vm, ssh_exec).WillOnce(DoDefault());

    std::chrono::milliseconds timeout(1);
    EXPECT_NO_THROW(vm.wait_for_cloud_init(timeout));
}

TEST_F(BaseVM, waitForCloudInitErrorTimesOutThrows)
{
    vm.simulate_cloud_init();
    EXPECT_CALL(vm, ensure_vm_is_running()).WillRepeatedly(Return());
    EXPECT_CALL(vm, ssh_exec).WillOnce(Throw(mp::SSHExecFailure{"no worky", 1}));

    std::chrono::milliseconds timeout(1);
    MP_EXPECT_THROW_THAT(vm.wait_for_cloud_init(timeout),
                         std::runtime_error,
                         mpt::match_what(StrEq("timed out waiting for initialization to complete")));
}

TEST_F(BaseVM, waitForSSHUpThrowsOnTimeout)
{
    vm.simulate_waiting_for_ssh();
    EXPECT_CALL(vm, ssh_hostname(_)).WillOnce(Throw(std::runtime_error{"intentional"}));

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    auto& mock_utils = *mock_utils_ptr;
    MP_DELEGATE_MOCK_CALLS_ON_BASE(mock_utils, sleep_for, mp::Utils);

    MP_EXPECT_THROW_THAT(vm.wait_until_ssh_up(std::chrono::milliseconds{1}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("timed out waiting for response")));
}

using ExceptionParam =
    std::variant<std::runtime_error, mp::IPUnavailableException, mp::SSHException, mp::InternalTimeoutException>;
class TestWaitForSSHExceptions : public BaseVM, public WithParamInterface<ExceptionParam>
{
};

TEST_P(TestWaitForSSHExceptions, waitForSSHUpRetriesOnExpectedException)
{
    static constexpr auto thrower = [](const auto& e) { throw e; };

    vm.simulate_waiting_for_ssh();
    EXPECT_CALL(vm, ensure_vm_is_running()).WillRepeatedly(Return());
    EXPECT_CALL(vm, update_state()).WillRepeatedly(Return());

    auto timeout = std::chrono::milliseconds{100};
    EXPECT_CALL(vm, ssh_hostname(_))
        .WillOnce(WithoutArgs([]() {
            std::visit(thrower, GetParam());
            return "neverland";
        }))
        .WillRepeatedly(Return("underworld"));

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, sleep_for(_)).WillRepeatedly(Return());

    EXPECT_NO_THROW(vm.wait_until_ssh_up(timeout));
}

INSTANTIATE_TEST_SUITE_P(TestWaitForSSHExceptions,
                         TestWaitForSSHExceptions,
                         Values(std::runtime_error{"todo-remove-eventually"},
                                mp::IPUnavailableException{"noip"},
                                mp::SSHException{"nossh"},
                                mp::InternalTimeoutException{"notime", std::chrono::seconds{1}}));

TEST_F(BaseVM, sshExecRefusesToExecuteIfVMIsNotRunning)
{
    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, is_running).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_utils_ptr, run_in_ssh_session).Times(0);

    vm.simulate_ssh_exec();
    MP_EXPECT_THROW_THAT(vm.ssh_exec("echo"), mp::SSHException, mpt::match_what(HasSubstr("not running")));
}

TEST_F(BaseVM, sshExecRunsDirectlyIfConnected)
{
    static constexpr auto* cmd = ":";

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, is_running).WillOnce(Return(true));
    EXPECT_CALL(*mock_utils_ptr, run_in_ssh_session(_, cmd, _)).Times(1);

    vm.simulate_ssh_exec();
    vm.renew_ssh_session();

    EXPECT_NO_THROW(vm.ssh_exec(cmd));
}

TEST_F(BaseVM, sshExecReconnectsIfDisconnected)
{
    static constexpr auto* cmd = ":";

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, is_running).WillOnce(Return(true));
    EXPECT_CALL(*mock_utils_ptr, run_in_ssh_session(_, cmd, _)).Times(1);

    vm.simulate_ssh_exec();

    EXPECT_NO_THROW(vm.ssh_exec(cmd));
}

TEST_F(BaseVM, sshExecTriesToReconnectAfterLateDetectionOfDisconnection)
{
    static constexpr auto* cmd = ":";

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, is_running).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_utils_ptr, run_in_ssh_session(_, cmd, _))
        .WillOnce(Throw(mp::SSHException{"intentional"}))
        .WillOnce(DoDefault());

    vm.simulate_ssh_exec();
    vm.renew_ssh_session();

    mock_ssh_test_fixture.is_connected.returnValue(true, false, false);

    EXPECT_NO_THROW(vm.ssh_exec(cmd));
}

TEST_F(BaseVM, sshExecRethrowsOtherExceptions)
{
    static constexpr auto* cmd = ":";

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, is_running).WillOnce(Return(true));
    EXPECT_CALL(*mock_utils_ptr, run_in_ssh_session(_, cmd, _)).WillOnce(Throw(std::runtime_error{"intentional"}));

    vm.simulate_ssh_exec();
    vm.renew_ssh_session();

    MP_EXPECT_THROW_THAT(vm.ssh_exec(cmd), std::runtime_error, mpt::match_what(HasSubstr("intentional")));
}

TEST_F(BaseVM, sshExecRethrowsSSHExceptionsWhenConnected)
{
    static constexpr auto* cmd = ":";

    auto [mock_utils_ptr, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils_ptr, is_running).WillOnce(Return(true));
    EXPECT_CALL(*mock_utils_ptr, run_in_ssh_session(_, cmd, _)).WillOnce(Throw(mp::SSHException{"intentional"}));

    vm.simulate_ssh_exec();
    vm.renew_ssh_session();

    MP_EXPECT_THROW_THAT(vm.ssh_exec(cmd), mp::SSHException, mpt::match_what(HasSubstr("intentional")));
}

} // namespace

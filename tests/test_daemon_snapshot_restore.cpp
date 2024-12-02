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
#include "daemon_test_fixture.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_snapshot.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"
#include "multipass/exceptions/snapshot_exceptions.h"

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestDaemonSnapshotRestoreBase : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    }

    auto build_daemon_with_mock_instance()
    {
        const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

        auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
        auto* ret_instance = instance_ptr.get();

        EXPECT_CALL(*instance_ptr, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::restarting));
        EXPECT_CALL(mock_factory, create_virtual_machine).WillOnce(Return(std::move(instance_ptr)));

        config_builder.data_directory = temp_dir->path();
        auto daemon = std::make_unique<mp::Daemon>(config_builder.build());

        return std::pair{std::move(daemon), ret_instance};
    }

    mpt::MockPlatform::GuardedMock mock_platform_injection{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform& mock_platform = *mock_platform_injection.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    mpt::MockVirtualMachineFactory& mock_factory = *use_a_mock_vm_factory();

    std::vector<mp::NetworkInterface> extra_interfaces;
    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string mock_instance_name{"real-zebraphant"};
};

struct TestDaemonSnapshot : public TestDaemonSnapshotRestoreBase
{
    using TestDaemonSnapshotRestoreBase::SetUp; // It seems this is what signals gtest the type to use for test names
};

struct TestDaemonRestore : public TestDaemonSnapshotRestoreBase
{
    using TestDaemonSnapshotRestoreBase::SetUp; // It seems this is what signals gtest the type to use for test names
};

struct SnapshotRPCTypes
{
    using Request = mp::SnapshotRequest;
    using Reply = mp::SnapshotReply;
    inline static constexpr auto daemon_slot_ptr = &mp::Daemon::snapshot;
};

struct RestoreRPCTypes
{
    using Request = mp::RestoreRequest;
    using Reply = mp::RestoreReply;
    inline static constexpr auto daemon_slot_ptr = &mp::Daemon::restore;
};

template <typename RPCTypes>
struct TestDaemonSnapshotRestoreCommon : public TestDaemonSnapshotRestoreBase
{
    using TestDaemonSnapshotRestoreBase::SetUp; // It seems this is what signals gtest the type to use for test names
    using MockServer = StrictMock<mpt::MockServerReaderWriter<typename RPCTypes::Reply, typename RPCTypes::Request>>;
};

using RPCTypes = Types<SnapshotRPCTypes, RestoreRPCTypes>;
TYPED_TEST_SUITE(TestDaemonSnapshotRestoreCommon, RPCTypes);

TYPED_TEST(TestDaemonSnapshotRestoreCommon, failsOnMissingInstance)
{
    static constexpr auto missing_instance = "foo";
    typename TypeParam::Request request{};
    request.set_instance(missing_instance);

    mp::Daemon daemon{this->config_builder.build()};
    auto status =
        this->call_daemon_slot(daemon, TypeParam::daemon_slot_ptr, request, typename TestFixture::MockServer{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(status.error_message(), fmt::format("instance \"{}\" does not exist", missing_instance));
}

TYPED_TEST(TestDaemonSnapshotRestoreCommon, failsOnActiveInstance)
{
    typename TypeParam::Request request{};
    request.set_instance(this->mock_instance_name);

    auto [daemon, instance] = this->build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::restarting));

    auto status =
        this->call_daemon_slot(*daemon, TypeParam::daemon_slot_ptr, request, typename TestFixture::MockServer{});

    EXPECT_EQ(status.error_code(), grpc::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(), HasSubstr("stopped"));
}

TEST_F(TestDaemonSnapshot, failsIfBackendDoesNotSupportSnapshots)
{
    EXPECT_CALL(mock_factory, require_snapshots_support)
        .WillRepeatedly(Throw(mp::NotImplementedOnThisBackendException{"snapshots"}));

    mp::Daemon daemon{config_builder.build()};
    auto status = call_daemon_slot(daemon,
                                   &mp::Daemon::snapshot,
                                   mp::SnapshotRequest{},
                                   StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_THAT(status.error_message(), AllOf(HasSubstr("not implemented"), HasSubstr("snapshots")));
}

TEST_F(TestDaemonSnapshot, failsOnInvalidSnapshotName)
{
    mp::SnapshotRequest request{};
    request.set_instance(mock_instance_name);
    request.set_snapshot("%$@#*& \t\n nope, no.can.do");

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::snapshot,
                                   request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr("Invalid snapshot name"));
}

TEST_F(TestDaemonSnapshot, failsOnRepeatedSnapshotName)
{
    static constexpr auto* snapshot_name = "Obelix";
    mp::SnapshotRequest request{};
    request.set_instance(mock_instance_name);
    request.set_snapshot(snapshot_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::off));
    EXPECT_CALL(*instance, take_snapshot(_, Eq(snapshot_name), _))
        .WillOnce(Throw(mp::SnapshotNameTakenException{mock_instance_name, snapshot_name}));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::snapshot,
                                   request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), AllOf(HasSubstr(mock_instance_name), HasSubstr(snapshot_name)));
}

TEST_F(TestDaemonSnapshot, usesProvidedSnapshotProperties)
{
    static constexpr auto* snapshot_name = "orangutan";
    static constexpr auto* snapshot_comment = "not a monkey";

    mp::SnapshotRequest request{};
    request.set_instance(mock_instance_name);
    request.set_snapshot(snapshot_name);
    request.set_comment(snapshot_comment);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));

    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, get_name).WillOnce(Return(snapshot_name));
    EXPECT_CALL(*instance, take_snapshot(_, Eq(snapshot_name), Eq(snapshot_comment))).WillOnce(Return(snapshot));

    auto server = StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{};
    EXPECT_CALL(server, Write(Property(&mp::SnapshotReply::snapshot, Eq(snapshot_name)), _)).WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::snapshot, request, server);

    EXPECT_EQ(status.error_code(), grpc::OK);
}

TEST_F(TestDaemonSnapshot, acceptsEmptySnapshotName)
{
    static constexpr auto* generated_name = "asdrubal";

    mp::SnapshotRequest request{};
    request.set_instance(mock_instance_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::off));

    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, get_name).WillOnce(Return(generated_name));
    EXPECT_CALL(*instance, take_snapshot(_, IsEmpty(), IsEmpty())).WillOnce(Return(snapshot));

    auto server = StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{};
    EXPECT_CALL(server, Write(Property(&mp::SnapshotReply::snapshot, Eq(generated_name)), _)).WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::snapshot, request, server);

    EXPECT_EQ(status.error_code(), grpc::OK);
}

TEST_F(TestDaemonRestore, failsIfBackendDoesNotSupportSnapshots)
{
    mp::RestoreRequest request{};
    request.set_instance(mock_instance_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*instance, get_snapshot(A<const std::string&>()))
        .WillOnce(Throw(mp::NotImplementedOnThisBackendException{"snapshots"}));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::restore,
                                   request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::RestoreReply, mp::RestoreRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_THAT(status.error_message(), AllOf(HasSubstr("not implemented"), HasSubstr("snapshots")));
}

TEST_F(TestDaemonRestore, failsOnMissingSnapshotName)
{
    static constexpr auto* missing_snapshot_name = "albatross";
    mp::RestoreRequest request{};
    request.set_instance(mock_instance_name);
    request.set_snapshot(missing_snapshot_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*instance, get_snapshot(TypedEq<const std::string&>(missing_snapshot_name)))
        .WillOnce(Throw(mp::NoSuchSnapshotException{mock_instance_name, missing_snapshot_name}));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::restore,
                                   request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::RestoreReply, mp::RestoreRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr("No such snapshot"), HasSubstr(mock_instance_name), HasSubstr(missing_snapshot_name)));
}

TEST_F(TestDaemonRestore, restoresSnapshotDirectlyIfDestructive)
{
    static constexpr auto* snapshot_name = "dodo";
    mp::RestoreRequest request{};
    request.set_instance(mock_instance_name);
    request.set_snapshot(snapshot_name);
    request.set_destructive(true);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*instance, restore_snapshot(Eq(snapshot_name), _)).Times(1);

    StrictMock<mpt::MockServerReaderWriter<mp::RestoreReply, mp::RestoreRequest>> server{};
    EXPECT_CALL(server, Write).Times(2);
    auto status = call_daemon_slot(*daemon, &mp::Daemon::restore, request, server);

    EXPECT_EQ(status.error_code(), grpc::OK);
}

} // namespace

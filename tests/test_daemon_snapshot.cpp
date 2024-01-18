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

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestDaemonSnapshot : public mpt::DaemonTestFixture
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

TEST_F(TestDaemonSnapshot, failsOnMissingInstance)
{
    static constexpr auto missing_instance = "foo";
    mp::SnapshotRequest request{};
    request.set_instance(missing_instance);

    mp::Daemon daemon{config_builder.build()};
    auto status = call_daemon_slot(daemon,
                                   &mp::Daemon::snapshot,
                                   request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(status.error_message(), fmt::format("instance \"{}\" does not exist", missing_instance));
}

TEST_F(TestDaemonSnapshot, failsOnActiveInstance)
{
    mp::SnapshotRequest request{};
    request.set_instance(mock_instance_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::restarting));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::snapshot,
                                   request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::INVALID_ARGUMENT);
    EXPECT_EQ(status.error_message(), "Multipass can only take snapshots of stopped instances.");
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

TEST_F(TestDaemonSnapshot, usesProvidedSnapshotName)
{
    mp::SnapshotRequest request{};
    request.set_instance(mock_instance_name);

    static constexpr auto* snapshot_name = "orangutan";
    request.set_snapshot(snapshot_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));

    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, get_name).WillOnce(Return(snapshot_name));
    EXPECT_CALL(*instance, take_snapshot(_, Eq(snapshot_name), IsEmpty())).WillOnce(Return(snapshot));

    auto server = StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{};
    EXPECT_CALL(server, Write(Property(&mp::SnapshotReply::snapshot, Eq(snapshot_name)), _)).WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::snapshot, request, server);

    EXPECT_EQ(status.error_code(), grpc::OK);
}

TEST_F(TestDaemonSnapshot, acceptsEmptySnapshotName)
{
    mp::SnapshotRequest request{};
    request.set_instance(mock_instance_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::off));

    static constexpr auto* generated_name = "asdrubal";
    auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    EXPECT_CALL(*snapshot, get_name).WillOnce(Return(generated_name));
    EXPECT_CALL(*instance, take_snapshot(_, IsEmpty(), IsEmpty())).WillOnce(Return(snapshot));

    auto server = StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{};
    EXPECT_CALL(server, Write(Property(&mp::SnapshotReply::snapshot, Eq(generated_name)), _)).WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::snapshot, request, server);

    EXPECT_EQ(status.error_code(), grpc::OK);
}
} // namespace

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

// The daemon test fixture contains premock code so it must be included first.
#include "daemon_test_fixture.h"

#include "common.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_snapshot.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <multipass/exceptions/snapshot_exceptions.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestDaemonDelete : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    }

    auto build_daemon_with_mock_instance()
    {
        const auto [temp_dir,
                    filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

        auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
        auto* ret_instance = instance_ptr.get();

        EXPECT_CALL(*instance_ptr, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
        EXPECT_CALL(mock_factory, create_virtual_machine).WillOnce(Return(std::move(instance_ptr)));

        config_builder.data_directory = temp_dir->path();
        auto daemon = std::make_unique<mp::Daemon>(config_builder.build());

        return std::pair{std::move(daemon), ret_instance};
    }

    mp::DeleteRequest make_snapshot_request(bool force) const
    {
        mp::DeleteRequest request{};
        auto* pair = request.add_instance_snapshot_pairs();
        pair->set_instance_name(mock_instance_name);
        pair->set_snapshot_name(snapshot_name);
        request.set_force(force);
        return request;
    }

    static constexpr auto* snapshot_name = "pelican";

    mpt::MockPlatform::GuardedMock mock_platform_injection{mpt::MockPlatform::inject<NiceMock>()};

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();

    mpt::MockVirtualMachineFactory& mock_factory = *use_a_mock_vm_factory();

    StrictMock<mpt::MockServerReaderWriter<mp::DeleteReply, mp::DeleteRequest>> server;

    std::vector<mp::NetworkInterface> extra_interfaces;
    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string mock_instance_name{"real-zebraphant"};
};

TEST_F(TestDaemonDelete, failsOnMissingSnapshotWithoutDeleting)
{
    static constexpr auto* missing_snapshot_name = "albatross";
    auto request = make_snapshot_request(/*force=*/false);
    auto* pair = request.add_instance_snapshot_pairs();
    pair->set_instance_name(mock_instance_name);
    pair->set_snapshot_name(missing_snapshot_name);

    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, get_snapshot(TypedEq<const std::string&>(snapshot_name)))
        .WillRepeatedly(Return(std::make_shared<NiceMock<mpt::MockSnapshot>>()));
    EXPECT_CALL(*instance, get_snapshot(TypedEq<const std::string&>(missing_snapshot_name)))
        .WillOnce(Throw(mp::NoSuchSnapshotException{mock_instance_name, missing_snapshot_name}));
    EXPECT_CALL(*instance, delete_snapshot).Times(0);

    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::deleted_instances_size, 0), _))
        .WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::delet, request, server);

    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr("No such snapshot"),
                      HasSubstr(mock_instance_name),
                      HasSubstr(missing_snapshot_name)));
}

TEST_F(TestDaemonDelete, failsOnMissingInstanceWithoutAskingForConfirmation)
{
    mp::DeleteRequest request{};
    request.add_instance_snapshot_pairs()->set_instance_name("nonexistent");

    auto [daemon, instance] = build_daemon_with_mock_instance();

    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, false), _))
        .WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::delet, request, server);

    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_THAT(status.error_message(), HasSubstr("does not exist"));
}

TEST_F(TestDaemonDelete, skipsConfirmationWhenForced)
{
    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, get_snapshot(TypedEq<const std::string&>(snapshot_name)))
        .WillRepeatedly(Return(std::make_shared<NiceMock<mpt::MockSnapshot>>()));
    EXPECT_CALL(*instance, delete_snapshot(snapshot_name));

    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, false), _))
        .WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::delet,
                                   make_snapshot_request(true),
                                   server);

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonDelete, deletesWhenClientConfirms)
{
    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, get_snapshot(TypedEq<const std::string&>(snapshot_name)))
        .WillRepeatedly(Return(std::make_shared<NiceMock<mpt::MockSnapshot>>()));
    EXPECT_CALL(*instance, delete_snapshot(snapshot_name));

    InSequence seq;
    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, true), _))
        .WillOnce(Return(true));
    EXPECT_CALL(server, Read).WillOnce([](mp::DeleteRequest* response) {
        response->set_force(true);
        return true;
    });
    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, false), _))
        .WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::delet,
                                   make_snapshot_request(false),
                                   server);

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonDelete, doesNotDeleteWhenClientDeclines)
{
    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, get_snapshot(TypedEq<const std::string&>(snapshot_name)))
        .WillRepeatedly(Return(std::make_shared<NiceMock<mpt::MockSnapshot>>()));
    EXPECT_CALL(*instance, delete_snapshot).Times(0);

    InSequence seq;
    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, true), _))
        .WillOnce(Return(true));
    EXPECT_CALL(server, Read).WillOnce(Return(true));
    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, false), _))
        .WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::delet,
                                   make_snapshot_request(false),
                                   server);

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonDelete, abortsWhenConfirmationUnavailable)
{
    auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, get_snapshot(TypedEq<const std::string&>(snapshot_name)))
        .WillRepeatedly(Return(std::make_shared<NiceMock<mpt::MockSnapshot>>()));
    EXPECT_CALL(*instance, delete_snapshot).Times(0);

    InSequence seq;
    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, true), _))
        .WillOnce(Return(true));
    EXPECT_CALL(server, Read).WillOnce(Return(false));
    EXPECT_CALL(server, Write(Property(&mp::DeleteReply::confirm_deletion, false), _))
        .WillOnce(Return(true));

    auto status = call_daemon_slot(*daemon,
                                   &mp::Daemon::delet,
                                   make_snapshot_request(false),
                                   server);

    EXPECT_EQ(status.error_code(), grpc::StatusCode::CANCELLED);
    EXPECT_THAT(status.error_message(), HasSubstr("Cannot get confirmation from client"));
}
} // namespace

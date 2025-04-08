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
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDaemonClone : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    }

    auto build_daemon_with_mock_instance()
    {
        auto instance_unique_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_src_instance_name);
        auto* instance_raw_ptr = instance_unique_ptr.get();

        EXPECT_CALL(mock_factory, create_virtual_machine).WillOnce(Return(std::move(instance_unique_ptr)));

        const auto [temp_dir, _] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
        config_builder.data_directory = temp_dir->path();
        auto daemon = std::make_unique<mp::Daemon>(config_builder.build());

        return std::pair{std::move(daemon), instance_raw_ptr};
    }

    const std::string mock_src_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};
    std::vector<mp::NetworkInterface> extra_interfaces;

    const mpt::MockVirtualMachineFactory& mock_factory = *use_a_mock_vm_factory();

    const mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    const mpt::MockPlatform& mock_platform = *attr.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;
};

TEST_F(TestDaemonClone, missingOnSrcInstance)
{
    const std::string src_instance_name = "non_exist_instance";
    mp::CloneRequest request{};
    request.set_source_name(src_instance_name);

    mp::Daemon daemon{config_builder.build()};
    const auto status = call_daemon_slot(daemon,
                                         &mp::Daemon::clone,
                                         request,
                                         NiceMock<mpt::MockServerReaderWriter<mp::CloneReply, mp::CloneRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
    EXPECT_EQ(status.error_message(), fmt::format("instance \"{}\" does not exist", src_instance_name));
}

TEST_F(TestDaemonClone, invalidDestVmName)
{
    const auto [daemon, instance] = build_daemon_with_mock_instance();

    constexpr std::string_view dest_instance_name = "5invalid_vm_name";
    mp::CloneRequest request{};
    request.set_source_name(mock_src_instance_name);
    request.set_destination_name(std::string{dest_instance_name});

    const auto status = call_daemon_slot(*daemon,
                                         &mp::Daemon::clone,
                                         request,
                                         NiceMock<mpt::MockServerReaderWriter<mp::CloneReply, mp::CloneRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr("Invalid destination instance name"));
}

TEST_F(TestDaemonClone, alreadyExistDestVmName)
{
    const auto [daemon, instance] = build_daemon_with_mock_instance();

    mp::CloneRequest request{};
    request.set_source_name(mock_src_instance_name);
    request.set_destination_name(mock_src_instance_name);

    const auto status = call_daemon_slot(*daemon,
                                         &mp::Daemon::clone,
                                         request,
                                         NiceMock<mpt::MockServerReaderWriter<mp::CloneReply, mp::CloneRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr("already exists"));
}

TEST_F(TestDaemonClone, successfulCloneGenerateDestNameOkStatus)
{
    // add this line to cover the update_unique_identifiers_of_metadata all branches
    extra_interfaces.emplace_back(mp::NetworkInterface{"eth1", "52:54:00:00:00:00", true});
    const auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillOnce(Return(mp::VirtualMachine::State::stopped));

    mp::CloneRequest request{};
    request.set_source_name(mock_src_instance_name);

    const auto status = call_daemon_slot(*daemon,
                                         &mp::Daemon::clone,
                                         request,
                                         NiceMock<mpt::MockServerReaderWriter<mp::CloneReply, mp::CloneRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::OK);
}

TEST_F(TestDaemonClone, successfulCloneSpecifyDestNameOkStatus)
{
    const auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillOnce(Return(mp::VirtualMachine::State::stopped));

    mp::CloneRequest request{};
    request.set_source_name(mock_src_instance_name);
    request.set_destination_name("valid-dest-instance-name");

    const auto status = call_daemon_slot(*daemon,
                                         &mp::Daemon::clone,
                                         request,
                                         NiceMock<mpt::MockServerReaderWriter<mp::CloneReply, mp::CloneRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::OK);
}

TEST_F(TestDaemonClone, failsOnCloneOnNonStoppedInstance)
{
    const auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillOnce(Return(mp::VirtualMachine::State::running));

    mp::CloneRequest request{};
    request.set_source_name(mock_src_instance_name);

    const auto status = call_daemon_slot(*daemon,
                                         &mp::Daemon::clone,
                                         request,
                                         NiceMock<mpt::MockServerReaderWriter<mp::CloneReply, mp::CloneRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_EQ(status.error_message(), fmt::format("Multipass can only clone stopped instances."));
}

TEST_F(TestDaemonClone, successfulCloneGenerateDestNameButThrowLater)
{
    const auto [daemon, instance] = build_daemon_with_mock_instance();
    EXPECT_CALL(*instance, current_state).WillOnce(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(mock_factory, clone_bare_vm).WillOnce(Throw(std::runtime_error("intentional")));

    mp::CloneRequest request{};
    request.set_source_name(mock_src_instance_name);

    const auto status = call_daemon_slot(*daemon,
                                         &mp::Daemon::clone,
                                         request,
                                         NiceMock<mpt::MockServerReaderWriter<mp::CloneReply, mp::CloneRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), "intentional");
}

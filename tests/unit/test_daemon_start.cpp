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
#include "mock_dns_provider.h"
#include "mock_mount_handler.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_virtual_machine_factory.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/exceptions/timeout_exception.h>
#include <multipass/format.h>

#include <functional>
#include <tuple>
#include <utility>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDaemonStart : public mpt::DaemonTestFixture
{
    using VMState = mp::VirtualMachine::State;
    using MockVM = NiceMock<mpt::MockVirtualMachine>;
    using ServerMock = StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>;

    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));
        EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return("senna"));

        mock_factory = use_a_mock_vm_factory();
    }

    std::unique_ptr<MockVM> make_mock_vm(VMState state)
    {
        auto mock_vm = std::make_unique<MockVM>();
        EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
        EXPECT_CALL(*mock_vm, current_state()).WillRepeatedly(Return(state));
        return mock_vm;
    }

    std::unique_ptr<mp::Daemon> make_daemon(
        std::unique_ptr<MockVM> mock_vm,
        const std::unordered_map<std::string, mp::VMMount>& mounts = {})
    {
        auto instance_data = plant_instance_json(
            fake_json_contents(mac_addr, extra_interfaces, mounts));

        EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

        instance_data_dir = std::move(instance_data.first);
        config_builder.data_directory = instance_data_dir->path();
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

        return std::make_unique<mp::Daemon>(config_builder.build());
    }

    mp::StartRequest make_start_request() const
    {
        mp::StartRequest request;
        request.mutable_instance_names()->add_instance_name(mock_instance_name);
        return request;
    }

    grpc::Status start_instance(mp::Daemon& daemon)
    {
        auto request = make_start_request();
        ServerMock server;
        EXPECT_CALL(server, Write(_, _)).Times(1);

        return call_daemon_slot(daemon, &mp::Daemon::start, request, std::move(server));
    }

    void expect_readiness_error(const grpc::Status& status,
                                grpc::StatusCode status_code,
                                mp::ReadinessError::ErrorCode readiness_error) const
    {
        ASSERT_EQ(status.error_code(), status_code);
        mp::StartError start_error;
        ASSERT_TRUE(start_error.ParseFromString(status.error_details()));
        const auto error = start_error.readiness_errors().find(mock_instance_name);
        ASSERT_NE(error, start_error.readiness_errors().end());
        EXPECT_EQ(error->second, readiness_error);
    }

    std::unordered_map<std::string, mp::VMMount> make_native_mounts() const
    {
        const mp::id_mappings uid_mappings{{1000, 1001}}, gid_mappings{{1002, 1003}};
        const mp::VMMount mount{fake_source_path,
                                gid_mappings,
                                uid_mappings,
                                mp::VMMount::MountType::Native};
        return {{fake_target_path, mount}};
    }

    const std::string mock_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string fake_target_path{"/home/luke/skywalker"};
    const std::string fake_source_path{"/home/han/solo"};
    std::vector<mp::NetworkInterface> extra_interfaces;
    mpt::MockVirtualMachineFactory* mock_factory = nullptr;
    std::unique_ptr<mpt::TempDir> instance_data_dir;

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;
};

TEST_F(TestDaemonStart, successfulStartOkStatus)
{
    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, start()).Times(1);

    auto daemon = make_daemon(std::move(mock_vm));
    auto status = start_instance(*daemon);

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStart, startedInstanceResolvesByName)
{
    const mp::IPAddress address{"10.1.2.3"};
    auto dns_provider = std::make_unique<NiceMock<mpt::MockDnsProvider>>();
    EXPECT_CALL(*dns_provider, set_host(Eq(mock_instance_name), ElementsAre(address)));
    config_builder.dns_provider = std::move(dns_provider);

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, management_ipv4).WillRepeatedly(Return(address));

    auto daemon = make_daemon(std::move(mock_vm));
    EXPECT_TRUE(start_instance(*daemon).ok());
}

TEST_F(TestDaemonStart, startSucceedsWhenInstanceCannotResolveByName)
{
    auto dns_provider = std::make_unique<NiceMock<mpt::MockDnsProvider>>();
    EXPECT_CALL(*dns_provider, set_host).WillOnce(Throw(std::runtime_error{"unavailable"}));
    config_builder.dns_provider = std::move(dns_provider);

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, management_ipv4).WillRepeatedly(Return(mp::IPAddress{"10.1.2.3"}));

    auto daemon = make_daemon(std::move(mock_vm));
    EXPECT_TRUE(start_instance(*daemon).ok());
}

TEST_F(TestDaemonStart, startedInstanceWithoutAnAddressDoesNotResolveByName)
{
    auto dns_provider = std::make_unique<NiceMock<mpt::MockDnsProvider>>();
    EXPECT_CALL(*dns_provider, set_host).Times(0);
    config_builder.dns_provider = std::move(dns_provider);

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, management_ipv4).WillRepeatedly(Return(std::nullopt));

    auto daemon = make_daemon(std::move(mock_vm));
    EXPECT_TRUE(start_instance(*daemon).ok());
}

TEST_F(TestDaemonStart, exitlessSshProcessExceptionDoesNotShowMessage)
{
    auto event_dopoll = [](auto...) { return SSH_ERROR; };
    REPLACE(ssh_event_dopoll, event_dopoll);

    extra_interfaces = {{"eth7", "52:54:00:99:99:99", true}};

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, start()).Times(1);
    // New networks configuration was moved to the instance settings handler,
    // add_network_interface mustn't be called.
    EXPECT_CALL(*mock_vm, add_network_interface(_, _, _)).Times(0);

    auto daemon = make_daemon(std::move(mock_vm));
    auto status = start_instance(*daemon);

    EXPECT_THAT(status.error_message(), StrEq(""));
    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStart, unknownStateDoesNotStart)
{
    auto mock_vm = make_mock_vm(VMState::unknown);
    EXPECT_CALL(*mock_vm, start()).Times(0);

    auto daemon = make_daemon(std::move(mock_vm));
    auto status = start_instance(*daemon);

    EXPECT_FALSE(status.ok());
}

TEST_F(TestDaemonStart, suspendingStateDoesNotStartHasError)
{
    auto mock_vm = make_mock_vm(VMState::suspending);
    EXPECT_CALL(*mock_vm, start()).Times(0);

    auto daemon = make_daemon(std::move(mock_vm));
    auto status = start_instance(*daemon);

    EXPECT_FALSE(status.ok());

    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("Cannot start the instance \'{}\' while suspending",
                                      mock_instance_name)));
}

TEST_F(TestDaemonStart, definedMountsInitializedDuringStart)
{
    auto mock_mount_handler = std::make_unique<mpt::MockMountHandler>();
    EXPECT_CALL(*mock_mount_handler, activate_impl).Times(1);

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, start).Times(1);
    EXPECT_CALL(*mock_vm, make_native_mount_handler)
        .WillOnce(Return(std::move(mock_mount_handler)));

    auto daemon = make_daemon(std::move(mock_vm), make_native_mounts());
    auto status = start_instance(*daemon);

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStart, removingMountOnFailedStart)
{
    auto error = "failed to start mount";
    auto mock_mount_handler = std::make_unique<mpt::MockMountHandler>();
    EXPECT_CALL(*mock_mount_handler, activate_impl).WillOnce(Throw(std::runtime_error{error}));

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillRepeatedly(Return());
    EXPECT_CALL(*mock_vm, start).Times(1);
    EXPECT_CALL(*mock_vm, make_native_mount_handler)
        .WillOnce(Return(std::move(mock_mount_handler)));

    auto daemon = make_daemon(std::move(mock_vm), make_native_mounts());

    auto log = fmt::format("Removing mount \"{}\" from '{}': {}\n",
                           fake_target_path,
                           mock_instance_name,
                           error);
    ServerMock server;

    EXPECT_CALL(server, Write(_, _));
    EXPECT_CALL(server, Write(Property(&mp::StartReply::log_line, Eq(log)), _));

    auto request = make_start_request();
    auto status = call_daemon_slot(*daemon, &mp::Daemon::start, request, std::move(server));
    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStart, sshfsMissingReturnsStructuredReadinessError)
{
    auto mock_mount_handler = std::make_unique<mpt::MockMountHandler>();
    EXPECT_CALL(*mock_mount_handler, activate_impl).WillOnce(Throw(mp::SSHFSMissingError{}));

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillOnce(Return());
    EXPECT_CALL(*mock_vm, start).Times(1);
    EXPECT_CALL(*mock_vm, make_native_mount_handler)
        .WillOnce(Return(std::move(mock_mount_handler)));

    auto daemon = make_daemon(std::move(mock_vm), make_native_mounts());
    const auto status = start_instance(*daemon);

    expect_readiness_error(status,
                           grpc::StatusCode::INVALID_ARGUMENT,
                           mp::ReadinessError::SSHFS_MISSING);
}

struct TestDaemonStartSshReadinessError
    : public TestDaemonStart,
      public WithParamInterface<
          std::tuple<mp::ReadinessError::ErrorCode, std::function<void()>, grpc::StatusCode>>
{
};

TEST_P(TestDaemonStartSshReadinessError, returnsStructuredReadinessError)
{
    const auto& [readiness_error, wait_ssh_func, grpc_status] = GetParam();

    auto mock_vm = make_mock_vm(VMState::stopped);
    EXPECT_CALL(*mock_vm, start).Times(1);
    EXPECT_CALL(*mock_vm, wait_until_ssh_up).WillOnce(wait_ssh_func);

    auto daemon = make_daemon(std::move(mock_vm));
    const auto status = start_instance(*daemon);

    expect_readiness_error(status, grpc_status, readiness_error);
}

INSTANTIATE_TEST_SUITE_P(TestDaemonStart,
                         TestDaemonStartSshReadinessError,
                         Values(std::make_tuple(
                                    mp::ReadinessError::UNPLUGGED,
                                    [] {
                                        throw mp::StartException{"real-zebraphant", "unplugged"};
                                    },
                                    grpc::StatusCode::INVALID_ARGUMENT),
                                std::make_tuple(
                                    mp::ReadinessError::SSH_TIMEOUT,
                                    [] { throw mp::SSHTimeoutException{"ssh timeout"}; },
                                    grpc::StatusCode::DEADLINE_EXCEEDED),
                                std::make_tuple(
                                    mp::ReadinessError::OTHER,
                                    [] { throw std::runtime_error{"unexpected error"}; },
                                    grpc::StatusCode::INVALID_ARGUMENT)));

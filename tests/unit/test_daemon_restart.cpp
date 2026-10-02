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
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <multipass/constants.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDaemonRestart : public mpt::DaemonTestFixture
{
    using VMState = mp::VirtualMachine::State;
    using ServerMock =
        StrictMock<mpt::MockServerReaderWriter<mp::RestartReply, mp::RestartRequest>>;

    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));

        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    }

    auto build_daemon_with_mock_instance(VMState state)
    {
        const auto [temp_dir, filename] =
            plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

        auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
        auto* ret_instance = instance_ptr.get();

        ON_CALL(*instance_ptr, get_name).WillByDefault(ReturnRef(mock_instance_name));
        EXPECT_CALL(*instance_ptr, current_state).WillRepeatedly(Return(state));
        EXPECT_CALL(mock_factory, create_virtual_machine).WillOnce(Return(std::move(instance_ptr)));

        config_builder.data_directory = temp_dir->path();
        auto daemon = std::make_unique<mp::Daemon>(config_builder.build());

        return std::pair{std::move(daemon), ret_instance};
    }

    mpt::MockPlatform::GuardedMock mock_platform_injection{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform& mock_platform = *mock_platform_injection.first;

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;

    mpt::MockVirtualMachineFactory& mock_factory = *use_a_mock_vm_factory();

    std::vector<mp::NetworkInterface> extra_interfaces;
    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string mock_instance_name{"real-zebraphant"};
};

TEST_F(TestDaemonRestart, successfulRestartOkStatus)
{
    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(mock_instance_name);
    auto [daemon, instance] = build_daemon_with_mock_instance(VMState::running);

    ServerMock mock_server{};
    EXPECT_CALL(mock_server, Write(_, _)).Times(1);

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, std::move(mock_server));

    EXPECT_EQ(status.error_code(), grpc::OK);
}

TEST_F(TestDaemonRestart, restartFailsOnMissingInstance)
{
    static constexpr auto missing_instance_name = "missing-instance";
    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(missing_instance_name);

    auto daemon = std::make_unique<mp::Daemon>(config_builder.build());
    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, ServerMock());

    EXPECT_EQ(status.error_code(), grpc::NOT_FOUND);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr(missing_instance_name), HasSubstr("does not exist")));
}

TEST_F(TestDaemonRestart, restartFailsOnStoppedInstanceWithRunningOnly)
{
    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(mock_instance_name);
    request.set_running_only(true);
    auto [daemon, instance] = build_daemon_with_mock_instance(VMState::stopped);

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, ServerMock());

    EXPECT_EQ(status.error_code(), grpc::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr(mock_instance_name), HasSubstr("is not running")));
}

TEST_F(TestDaemonRestart, restartFailsOnUnknownInstanceState)
{
    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(mock_instance_name);
    auto [daemon, instance] = build_daemon_with_mock_instance(VMState::unknown);

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, ServerMock());

    EXPECT_EQ(status.error_code(), grpc::FAILED_PRECONDITION);
    EXPECT_THAT(
        status.error_message(),
        AllOf(HasSubstr(mock_instance_name), HasSubstr("is 'unknown' and cannot be restarted")));
}

namespace
{
using State = mp::VirtualMachine::State;
using StateStatusPair = std::pair<State, grpc::Status>;
struct TestRestartOnDifferentStates : public TestDaemonRestart,
                                      public WithParamInterface<StateStatusPair>
{
};
} // namespace

TEST_P(TestRestartOnDifferentStates, restartOnStateWithoutRunningOnly)
{
    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(mock_instance_name);
    request.set_running_only(false);
    auto [state, expected_status] = GetParam();
    auto [daemon, instance] = build_daemon_with_mock_instance(state);
    instance->state = state;

    EXPECT_CALL(*instance, current_state()).WillRepeatedly([instance] { return instance->state; });
    if (state == mp::VirtualMachine::State::suspended)
        EXPECT_CALL(*instance, wait_until_ssh_up(_)).Times(2).WillRepeatedly([instance](auto...) {
            instance->state = mp::VirtualMachine::State::running;
        });

    ServerMock mock_server{};
    EXPECT_CALL(mock_server, Write(_, _)).WillRepeatedly(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, std::move(mock_server));

    EXPECT_EQ(status.error_code(), expected_status.error_code());
}

TEST_P(TestRestartOnDifferentStates, restartOnStateWithRunningOnly)
{
    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(mock_instance_name);
    request.set_running_only(true);
    auto [state, expected_status] = GetParam();
    bool is_not_running{state != State::running && state != State::delayed_shutdown};
    auto [daemon, instance] = build_daemon_with_mock_instance(state);

    ServerMock mock_server{};
    EXPECT_CALL(mock_server, Write(_, _)).WillRepeatedly(Return(true));

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, std::move(mock_server));

    if (is_not_running)
        expected_status = grpc::Status{grpc::StatusCode::FAILED_PRECONDITION, "", ""};
    EXPECT_EQ(status.error_code(), expected_status.error_code());
}

INSTANTIATE_TEST_SUITE_P(
    TestDaemonRestart,
    TestRestartOnDifferentStates,
    Values(StateStatusPair{State::running, grpc::Status::OK},
           StateStatusPair{State::stopped, grpc::Status::OK},
           StateStatusPair{State::off, grpc::Status::OK},
           StateStatusPair{State::suspended, grpc::Status::OK},
           StateStatusPair{State::delayed_shutdown, grpc::Status::OK},
           StateStatusPair{State::restarting, grpc::Status(grpc::FAILED_PRECONDITION, "", "")},
           StateStatusPair{State::starting, grpc::Status(grpc::FAILED_PRECONDITION, "", "")},
           StateStatusPair{State::suspending, grpc::Status(grpc::FAILED_PRECONDITION, "", "")},
           StateStatusPair{State::unavailable, grpc::Status(grpc::FAILED_PRECONDITION, "", "")},
           StateStatusPair{State::unknown, grpc::Status(grpc::FAILED_PRECONDITION, "", "")}));

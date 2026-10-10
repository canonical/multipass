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
#include <multipass/exceptions/ssh_exception.h>

#include <deque>

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

    auto build_daemon_with_two_mock_instances(std::string name1,
                                              VMState state1,
                                              std::string name2,
                                              VMState state2)
    {
        static constexpr auto two_instance_template = R"(
"{}": {{
    "deleted": false,
    "disk_space": "3232323232",
    "mac_addr": "ab:cd:ef:12:34:{}",
    "mem_size": "2323232323",
    "metadata": {{}},
    "mounts": [],
    "num_cores": 4,
    "ssh_username": "ubuntu",
    "state": 1
}})";
        auto instance1_json = fmt::format(two_instance_template, name1, "56");
        auto instance2_json = fmt::format(two_instance_template, name2, "78");
        const auto [temp_dir, filename] = plant_instance_json(
            fmt::format("{{\n{},\n{}\n}}", instance1_json, instance2_json));

        // Names are stored in the fixture so the mocks can safely return references to them for
        // the lifetime of the test.
        const auto& stored_name1 = instance_name_storage.emplace_back(std::move(name1));
        const auto& stored_name2 = instance_name_storage.emplace_back(std::move(name2));

        auto instance1_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
        auto* ret_instance1 = instance1_ptr.get();
        auto instance2_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
        auto* ret_instance2 = instance2_ptr.get();

        // current_state() reflects the mutable `state` member (inherited from VirtualMachine), so
        // tests can change an instance's state mid-flight (e.g. once it resumes from suspension).
        instance1_ptr->state = state1;
        instance2_ptr->state = state2;
        ON_CALL(*instance1_ptr, get_name).WillByDefault(ReturnRef(stored_name1));
        EXPECT_CALL(*instance1_ptr, current_state).WillRepeatedly([ret_instance1] {
            return ret_instance1->state;
        });
        ON_CALL(*instance2_ptr, get_name).WillByDefault(ReturnRef(stored_name2));
        EXPECT_CALL(*instance2_ptr, current_state).WillRepeatedly([ret_instance2] {
            return ret_instance2->state;
        });

        EXPECT_CALL(
            mock_factory,
            create_virtual_machine(Field(&mp::VirtualMachineDescription::vm_name, stored_name1),
                                   _,
                                   _))
            .WillOnce(Return(std::move(instance1_ptr)));
        EXPECT_CALL(
            mock_factory,
            create_virtual_machine(Field(&mp::VirtualMachineDescription::vm_name, stored_name2),
                                   _,
                                   _))
            .WillOnce(Return(std::move(instance2_ptr)));

        config_builder.data_directory = temp_dir->path();
        auto daemon = std::make_unique<mp::Daemon>(config_builder.build());

        return std::tuple{std::move(daemon), ret_instance1, ret_instance2};
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
    std::deque<std::string> instance_name_storage;
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

// The following tests exercise restart's multi-instance error handling: when one instance's VM
// operation fails while the command is still processing other instances, the daemon must not
// abort early. It should keep acting on the remaining instances and aggregate every failure into
// the final status.

TEST_F(TestDaemonRestart, restartContinuesAfterOneInstanceFailsRebootAndAggregatesStatus)
{
    const std::string good_name{"good-zebraphant"};
    const std::string bad_name{"bad-zebraphant"};

    auto [daemon, good_instance, bad_instance] = build_daemon_with_two_mock_instances(
        good_name,
        VMState::running,
        bad_name,
        VMState::running);

    // The "bad" instance fails while trying to issue the reboot command over SSH, the "good"
    // instance succeeds.
    EXPECT_CALL(*bad_instance, ssh_exec(_, _)).WillRepeatedly(Return(""));
    EXPECT_CALL(*bad_instance, ssh_exec(StrEq("sudo reboot"), _))
        .WillOnce(Throw(mp::SSHExecFailure{"reboot command failed", 1}));
    EXPECT_CALL(*good_instance, ssh_exec(_, _)).WillRepeatedly(Return(""));
    EXPECT_CALL(*good_instance, ssh_exec(StrEq("sudo reboot"), _)).WillOnce(Return(""));

    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(good_name);
    request.mutable_instance_names()->add_instance_name(bad_name);

    ServerMock mock_server{};
    // Only the successfully-rebooted "good" instance gets waited on and written back.
    EXPECT_CALL(mock_server, Write(_, _)).Times(1);

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, std::move(mock_server));

    // Since at least one instance (the "good" one) got far enough to be waited on for
    // readiness, the aggregated error from the "bad" instance is folded into that pipeline's
    // error reporting, which reports it as INVALID_ARGUMENT rather than FAILED_PRECONDITION.
    EXPECT_EQ(status.error_code(), grpc::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr("Reboot command exited with code 1"));
}

TEST_F(TestDaemonRestart, restartAggregatesFailuresFromMultipleInstances)
{
    const std::string first_name{"first-zebraphant"};
    const std::string second_name{"second-zebraphant"};

    auto [daemon, first_instance, second_instance] = build_daemon_with_two_mock_instances(
        first_name,
        VMState::unavailable,
        second_name,
        VMState::unavailable);

    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(first_name);
    request.mutable_instance_names()->add_instance_name(second_name);

    ServerMock mock_server{};

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, std::move(mock_server));

    EXPECT_EQ(status.error_code(), grpc::FAILED_PRECONDITION);
    // Both instances' individual failure messages should show up in the aggregated status.
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr(first_name), HasSubstr(second_name), HasSubstr("is unavailable")));
}

TEST_F(TestDaemonRestart, restartFailsWhenSuspendedInstanceFailsToResume)
{
    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(mock_instance_name);
    auto [daemon, instance] = build_daemon_with_mock_instance(VMState::suspended);

    // The instance never comes back up over SSH, so it remains suspended throughout.
    EXPECT_CALL(*instance, wait_until_ssh_up(_))
        .WillOnce(Throw(std::runtime_error{"ssh timed out resuming instance"}));

    ServerMock mock_server{};

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, std::move(mock_server));

    EXPECT_EQ(status.error_code(), grpc::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr("ssh timed out resuming instance"),
                      HasSubstr("failed to resume and restart")));
}

TEST_F(TestDaemonRestart, restartSucceedsForOtherInstancesWhenOneSuspendedInstanceFailsToResume)
{
    const std::string resumes_ok_name{"resumes-ok-zebraphant"};
    const std::string fails_resume_name{"fails-resume-zebraphant"};

    auto [daemon,
          resumes_ok_instance,
          fails_resume_instance] = build_daemon_with_two_mock_instances(resumes_ok_name,
                                                                        VMState::suspended,
                                                                        fails_resume_name,
                                                                        VMState::suspended);

    // The first instance successfully resumes (comes back up over SSH); it is then waited on a
    // second time once rebooted, as part of the post-reboot wait-for-ready pipeline.
    EXPECT_CALL(*resumes_ok_instance, wait_until_ssh_up(_))
        .Times(2)
        .WillRepeatedly(
            Invoke([resumes_ok_instance](auto) { resumes_ok_instance->state = VMState::running; }));
    // ... while the second one never comes back up over SSH and remains suspended.
    EXPECT_CALL(*fails_resume_instance, wait_until_ssh_up(_))
        .WillOnce(Throw(std::runtime_error{"ssh timed out resuming instance"}));

    mp::RestartRequest request{};
    request.mutable_instance_names()->add_instance_name(resumes_ok_name);
    request.mutable_instance_names()->add_instance_name(fails_resume_name);

    ServerMock mock_server{};
    // The resumed instance still proceeds through the reboot + wait-for-ready pipeline.
    EXPECT_CALL(mock_server, Write(_, _)).Times(1);

    auto status = call_daemon_slot(*daemon, &mp::Daemon::restart, request, std::move(mock_server));

    // As in the single-failure case above, the resumed instance reaching the wait-for-ready
    // pipeline causes the other instance's aggregated failure to be reported as INVALID_ARGUMENT.
    EXPECT_EQ(status.error_code(), grpc::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr("ssh timed out resuming instance"), HasSubstr(fails_resume_name)));
}

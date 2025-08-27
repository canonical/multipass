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

        auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
        auto* ret_instance = instance_ptr.get();

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

    StrictMock<mpt::MockServerReaderWriter<mp::RestartReply, mp::RestartRequest>> mock_server{};
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
    auto status = call_daemon_slot(
        *daemon,
        &mp::Daemon::restart,
        request,
        StrictMock<mp::test::MockServerReaderWriter<mp::RestartReply, mp::RestartRequest>>());

    EXPECT_EQ(status.error_code(), grpc::NOT_FOUND);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr(missing_instance_name), HasSubstr("does not exist")));
}

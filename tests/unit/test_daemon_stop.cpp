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
#include "mock_mount_handler.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestDaemonStop : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());

        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    }

    void plant_instance_with_mount(std::unique_ptr<mpt::MockVirtualMachine> mock_vm,
                                   std::unique_ptr<mpt::MockMountHandler> mock_mount_handler)
    {
        auto mock_factory = use_a_mock_vm_factory();
        std::unordered_map<std::string, mp::VMMount> mounts{
            {fake_target_path, {"foo", {}, {}, mp::VMMount::MountType::Native}}};

        std::tie(temp_dir,
                 std::ignore) = plant_instance_json(fake_json_contents(mac_addr, {}, mounts));
        config_builder.data_directory = temp_dir->path();

        EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
        EXPECT_CALL(*mock_vm, make_native_mount_handler(fake_target_path, _))
            .WillOnce(Return(std::move(mock_mount_handler)));

        EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));
    }

    const std::string mock_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string fake_target_path{"/home/ubuntu/foo"};
    std::unique_ptr<mpt::TempDir> temp_dir;

    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;
};
} // namespace

TEST_F(TestDaemonStop, forceStopStopsMountsAfterPoweringOff)
{
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(
        mp::VirtualMachine::State::running);
    auto mock_mount_handler = std::make_unique<mpt::MockMountHandler>();
    {
        InSequence seq;
        EXPECT_CALL(*mock_vm, shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff));
        EXPECT_CALL(*mock_mount_handler, is_active).WillOnce(Return(true));
        EXPECT_CALL(*mock_mount_handler, deactivate_impl(true));
    }

    plant_instance_with_mount(std::move(mock_vm), std::move(mock_mount_handler));
    mp::Daemon daemon{config_builder.build()};

    mp::StopRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);
    request.set_force_stop(true);

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::stop,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::StopReply, mp::StopRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStop, stopStopsMountsBeforeShuttingDown)
{
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(
        mp::VirtualMachine::State::running);
    auto mock_mount_handler = std::make_unique<mpt::MockMountHandler>();
    {
        InSequence seq;
        EXPECT_CALL(*mock_mount_handler, is_active).WillOnce(Return(true));
        EXPECT_CALL(*mock_mount_handler, deactivate_impl(true));
        EXPECT_CALL(*mock_vm, shutdown);
    }

    plant_instance_with_mount(std::move(mock_vm), std::move(mock_mount_handler));
    mp::Daemon daemon{config_builder.build()};

    mp::StopRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::stop,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::StopReply, mp::StopRequest>>{});

    EXPECT_TRUE(status.ok());
}

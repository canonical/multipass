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
#include "mock_virtual_machine_factory.h"
#include "mock_vm_image_vault.h"

#include <multipass/exceptions/mount_exceptions.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
const std::string fake_target_path{"/home/ubuntu/foo"};

struct TestDaemonUmount : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());

        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

        mock_factory = use_a_mock_vm_factory();
    }

    std::unique_ptr<mp::Daemon> make_daemon(std::unique_ptr<mpt::MockVirtualMachine> mock_vm)
    {
        auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, {}));
        instance_data_dir = std::move(temp_dir);
        config_builder.data_directory = instance_data_dir->path();

        EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
        EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));
        return std::make_unique<mp::Daemon>(config_builder.build());
    }

    grpc::Status unmount(mp::Daemon& daemon, const std::vector<std::string>& target_paths)
    {
        mp::UmountRequest request;
        for (const auto& target_path : target_paths)
        {
            auto entry = request.add_target_paths();
            entry->set_instance_name(mock_instance_name);
            entry->set_target_path(target_path);
        }

        return call_daemon_slot(
            daemon,
            &mp::Daemon::umount,
            request,
            StrictMock<mpt::MockServerReaderWriter<mp::UmountReply, mp::UmountRequest>>{});
    }

    grpc::Status unmount(mp::Daemon& daemon, const std::string& target_path = {})
    {
        return unmount(daemon, std::vector{target_path});
    }

    void failing_unmount(mp::Daemon& daemon,
                         const std::string& target_path,
                         const std::string& expected_error)
    {
        const auto status = unmount(daemon, target_path);
        EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
        EXPECT_THAT(status.error_message(), HasSubstr(expected_error));
    }

    std::string not_mounted_error(const std::string& target_path) const
    {
        return fmt::format("path \"{}\" is not mounted in '{}'", target_path, mock_instance_name);
    }

    std::string failed_to_unmount_error(const std::string& target_path,
                                        const std::string& error) const
    {
        return fmt::format("failed to unmount \"{}\" from '{}': {}",
                           target_path,
                           mock_instance_name,
                           error);
    }

    mpt::MockVirtualMachineFactory* mock_factory;

    const std::string mock_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};

    std::unique_ptr<mpt::TempDir> instance_data_dir;

    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;
};

struct TestDaemonUmountFailure : TestDaemonUmount, WithParamInterface<std::string>
{
};
} // namespace

TEST_F(TestDaemonUmount, missingInstanceFails)
{
    mp::Daemon daemon{config_builder.build()};

    const auto fake_instance = "fake";
    mp::UmountRequest request;
    auto entry = request.add_target_paths();
    entry->set_instance_name(fake_instance);

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::umount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::UmountReply, mp::UmountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("instance '{}' does not exist", fake_instance)));
}

TEST_F(TestDaemonUmount, noTargetsUnmountsAll)
{
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, remove_all_mounts);
    EXPECT_CALL(*mock_vm, remove_mount).Times(0);
    auto daemon = make_daemon(std::move(mock_vm));
    EXPECT_TRUE(unmount(*daemon).ok());
}

TEST_F(TestDaemonUmount, removesOnlyRequestedTargets)
{
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path));
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path + "2")).Times(0);
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path + "3"));
    EXPECT_CALL(*mock_vm, remove_all_mounts).Times(0);
    auto daemon = make_daemon(std::move(mock_vm));
    EXPECT_TRUE(unmount(*daemon, {fake_target_path, fake_target_path + "3"}).ok());
}

TEST_F(TestDaemonUmount, umountExpandsTildes)
{
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path));
    auto daemon = make_daemon(std::move(mock_vm));
    EXPECT_TRUE(unmount(*daemon, "~/foo").ok());
}

TEST_F(TestDaemonUmount, mountNotFound)
{
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path))
        .WillOnce(Throw(mp::MountNotFoundError{fake_target_path, mock_instance_name}));
    auto daemon = make_daemon(std::move(mock_vm));
    failing_unmount(*daemon, fake_target_path, not_mounted_error(fake_target_path));
}

TEST_F(TestDaemonUmount, failedMountDoesNotPreventUnmountingOtherTargets)
{
    const auto error = "device is busy";
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path))
        .WillOnce(Throw(std::runtime_error{error}));
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path + "2"));
    EXPECT_CALL(*mock_vm, remove_mount(fake_target_path + "3"));
    auto daemon = make_daemon(std::move(mock_vm));
    const auto status = unmount(*daemon,
                                {fake_target_path, fake_target_path + "2", fake_target_path + "3"});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(failed_to_unmount_error(fake_target_path, error)));
}

TEST_P(TestDaemonUmountFailure, reportsUnmountFailures)
{
    const auto error = "device is busy";
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    if (GetParam().empty())
    {
        EXPECT_CALL(*mock_vm, remove_all_mounts)
            .WillOnce(Return(std::vector{std::pair{fake_target_path, std::string{error}}}));
    }
    else
    {
        EXPECT_CALL(*mock_vm, remove_mount(fake_target_path))
            .WillOnce(Throw(std::runtime_error{error}));
    }

    auto daemon = make_daemon(std::move(mock_vm));
    failing_unmount(*daemon, GetParam(), failed_to_unmount_error(fake_target_path, error));
}

INSTANTIATE_TEST_SUITE_P(AllAndSingleTarget,
                         TestDaemonUmountFailure,
                         Values(std::string{}, fake_target_path));

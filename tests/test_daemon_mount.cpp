/*
 * Copyright (C) 2022 Canonical, Ltd.
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
#include "mock_logger.h"
#include "mock_mount_handler.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <multipass/constants.h>
#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/exceptions/sshfs_missing_error.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestDaemonMount : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler(_)).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));

        config_builder.mount_handlers.clear();
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

        mock_factory = use_a_mock_vm_factory();
    }

    std::unique_ptr<mpt::MockMountHandler> mock_mount_handler{std::make_unique<mpt::MockMountHandler>()};
    mpt::MockVirtualMachineFactory* mock_factory;

    const std::string mock_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string fake_target_path{"/home/ubuntu/foo"};

    std::vector<mp::NetworkInterface> extra_interfaces;
    mpt::TempDir mount_dir;

    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};
} // namespace

TEST_F(TestDaemonMount, refusesDisabledMount)
{
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("false"));

    std::stringstream err_stream;

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, mp::MountRequest{},
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(), HasSubstr("Mounts are disabled on this installation of Multipass."));
}

TEST_F(TestDaemonMount, missingSourceDirFails)
{
    const std::string missing_dir{"/missing/dir"};
    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(missing_dir);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr(fmt::format("source \"{}\" does not exist", missing_dir)));
}

TEST_F(TestDaemonMount, sourceNotDirFails)
{
    mpt::TempFile file;

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(file.name().toStdString());

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr(fmt::format("source \"{}\" is not a directory", file.name())));
}

TEST_F(TestDaemonMount, missingInstanceFails)
{
    const std::string& fake_instance{"fake"};

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(fake_instance);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr(fmt::format("instance \"{}\" does not exist", fake_instance)));
}

TEST_F(TestDaemonMount, invalidTargetPathFails)
{
    const std::string invalid_path{"/dev/foo"};
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(invalid_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr(fmt::format("Unable to mount to \"{}\"", invalid_path)));
}

TEST_F(TestDaemonMount, mountExistsDoesNotTryMount)
{
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*mock_mount_handler, has_instance_already_mounted(_, _)).WillOnce(Return(true));
    config_builder.mount_handlers[mp::VMMount::MountType::SSHFS] = std::move(mock_mount_handler);

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("\"{}:{}\" is already mounted", mock_instance_name, fake_target_path)));
}

TEST_F(TestDaemonMount, skipStartMountIfInstanceIsNotRunning)
{
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));

    EXPECT_CALL(*mock_mount_handler, has_instance_already_mounted(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_mount_handler, init_mount(_, _, _)).Times(1);
    EXPECT_CALL(*mock_mount_handler, start_mount(_, _, _, _)).Times(0);
    config_builder.mount_handlers[mp::VMMount::MountType::SSHFS] = std::move(mock_mount_handler);

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonMount, mountAlreadyDefinedLogsAndContinues)
{
    std::unordered_map<std::string, mp::VMMount> mounts;
    mounts.emplace(fake_target_path,
                   mp::VMMount{mount_dir.path().toStdString(), {}, {}, mp::VMMount::MountType::SSHFS});

    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces, mounts));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));

    EXPECT_CALL(*mock_mount_handler, has_instance_already_mounted(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_mount_handler, init_mount(_, _, _)).Times(1);
    EXPECT_CALL(*mock_mount_handler, start_mount(_, _, _, _)).Times(0);
    config_builder.mount_handlers[mp::VMMount::MountType::SSHFS] = std::move(mock_mount_handler);

    mp::Daemon daemon{config_builder.build()};

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    logger_scope.mock_logger->expect_log(
        mpl::Level::info, fmt::format("Mount already defined for \"{}:{}\"", mock_instance_name, fake_target_path));

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonMount, startsMountIfInstanceRunning)
{
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::running));

    EXPECT_CALL(*mock_mount_handler, has_instance_already_mounted(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_mount_handler, init_mount(_, _, _)).Times(1);
    EXPECT_CALL(*mock_mount_handler, start_mount(_, _, _, _)).Times(1);
    config_builder.mount_handlers[mp::VMMount::MountType::SSHFS] = std::move(mock_mount_handler);

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonMount, mountFailsSshfsMissing)
{
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::running));

    EXPECT_CALL(*mock_mount_handler, has_instance_already_mounted(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_mount_handler, init_mount(_, _, _)).Times(1);
    EXPECT_CALL(*mock_mount_handler, start_mount(_, _, _, _)).WillOnce(Throw(mp::SSHFSMissingError{}));
    config_builder.mount_handlers[mp::VMMount::MountType::SSHFS] = std::move(mock_mount_handler);

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("Error enabling mount support in '{}'", mock_instance_name)));
}

TEST_F(TestDaemonMount, mountFailsErrorMounting)
{
    const std::string error_msg{fmt::format("Cannot mount {}", mount_dir.path().toStdString())};
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::running));

    EXPECT_CALL(*mock_mount_handler, has_instance_already_mounted(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_mount_handler, init_mount(_, _, _)).Times(1);
    EXPECT_CALL(*mock_mount_handler, start_mount(_, _, _, _)).WillOnce(Throw(std::runtime_error(error_msg)));
    config_builder.mount_handlers[mp::VMMount::MountType::SSHFS] = std::move(mock_mount_handler);

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("error mounting \"{}\": {}", fake_target_path, error_msg)));
}

TEST_F(TestDaemonMount, expectedUidsGidsPassedToInitMount)
{
    const auto host_uid{1000}, instance_uid{1001}, host_gid{1002}, instance_gid{1003};
    const mp::VMMount mount{mount_dir.path().toStdString(), mp::id_mappings{{host_gid, instance_gid}},
                            mp::id_mappings{{host_uid, instance_uid}}, mp::VMMount::MountType::SSHFS};

    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));

    EXPECT_CALL(*mock_mount_handler, has_instance_already_mounted(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_mount_handler, init_mount(_, _, mount)).Times(1);
    config_builder.mount_handlers[mp::VMMount::MountType::SSHFS] = std::move(mock_mount_handler);

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto mount_maps = request.mutable_mount_maps();
    auto uid_pair = mount_maps->add_uid_mappings();
    uid_pair->set_host_id(host_uid);
    uid_pair->set_instance_id(instance_uid);
    auto gid_pair = mount_maps->add_gid_mappings();
    gid_pair->set_host_id(host_gid);
    gid_pair->set_instance_id(instance_gid);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonMount, performanceMountsNotImplementedHasErrorFails)
{
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*mock_factory, create_performance_mount_handler(_))
        .WillOnce(Throw(mp::NotImplementedOnThisBackendException("foo")));

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    request.set_mount_type(mp::MountRequest_MountType_NATIVE);
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(daemon, &mp::Daemon::mount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(), StrEq("The experimental mounts feature is not implemented on this backend."));
}

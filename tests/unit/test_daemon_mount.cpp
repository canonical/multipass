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
#include "json_test_utils.h"
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_mount_handler.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_virtual_machine_factory.h"
#include "mock_vm_image_vault.h"
#include "stub_availability_zone.h"
#include "stub_availability_zone_manager.h"
#include "stub_mount_handler.h"
#include "temp_dir.h"

#include <multipass/constants.h>
#include <multipass/exceptions/mount_exceptions.h>
#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/utils.h>
#include <shared/base_virtual_machine.h>

#include <QCoreApplication>

#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct MountTestVirtualMachine : mpt::MockVirtualMachineT<mp::BaseVirtualMachine>
{
    using MockVirtualMachineT::MockVirtualMachineT;
    using mp::BaseVirtualMachine::load_mounts;

    MOCK_METHOD(void, resize_disk_impl, (const mp::MemorySize&), (override));
};

struct TestDaemonMount : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler(_)).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));

        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

        mock_factory = use_a_mock_vm_factory();
        config_builder.az_manager = std::make_unique<mpt::StubAvailabilityZoneManager>();
    }

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

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;
    mpt::StubAvailabilityZone zone{};
};

struct TestDaemonMountState : TestDaemonMount, WithParamInterface<mp::VirtualMachine::State>
{
};
} // namespace

TEST_F(TestDaemonMount, refusesDisabledMount)
{
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("false"));

    std::stringstream err_stream;

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        mp::MountRequest{},
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(),
                HasSubstr("Mounts are disabled on this installation of Multipass."));
}

TEST_F(TestDaemonMount, missingInstanceFails)
{
    const std::string& fake_instance{"fake"};

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(fake_instance);

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("instance '{}' does not exist", fake_instance)));
}

TEST_F(TestDaemonMount, invalidTargetPathFails)
{
    const auto [temp_dir, _] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};

    const std::string invalid_path{"/dev/foo"};
    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(invalid_path);

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Invalid target path: /dev/foo");

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("unable to mount to \"{}\"", invalid_path)));
}

TEST_F(TestDaemonMount, mountIgnoresTrailingSlash)
{
    const auto [temp_dir,
                filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(*mock_vm, add_mount(fake_target_path, _, _))
        .WillOnce(Return())
        .WillOnce(Throw(mp::MountTargetInUseError{fake_target_path, mock_instance_name}));
    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    request.set_mount_type(mp::MountRequest::NATIVE);
    auto entry1 = request.add_target_paths();
    entry1->set_instance_name(mock_instance_name);
    entry1->set_target_path(fake_target_path);
    auto entry2 = request.add_target_paths();
    entry2->set_instance_name(mock_instance_name);
    entry2->set_target_path(fake_target_path + "/");

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("\"{}\" is already mounted in '{}'",
                                      fake_target_path,
                                      mock_instance_name)));
}

TEST_F(TestDaemonMount, mountExpandsTildes)
{
    const auto [temp_dir,
                filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(*mock_vm, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*mock_vm, add_mount(fake_target_path, _, _));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    request.set_mount_type(mp::MountRequest::NATIVE);
    auto entry1 = request.add_target_paths();
    entry1->set_instance_name(mock_instance_name);
    entry1->set_target_path("~/foo");

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_P(TestDaemonMountState, delegatesMountForAvailableInstance)
{
    const auto [temp_dir,
                filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(*mock_vm, current_state).WillRepeatedly(Return(GetParam()));
    const mp::VMMount mount{mount_dir.path().toStdString(), {}, {}, mp::VMMount::MountType::Native};
    EXPECT_CALL(*mock_vm, add_mount(fake_target_path, Eq(mount), _));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    request.set_mount_type(mp::MountRequest::NATIVE);
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_TRUE(status.ok());
}

INSTANTIATE_TEST_SUITE_P(StoppedAndRunning,
                         TestDaemonMountState,
                         Values(mp::VirtualMachine::State::stopped,
                                mp::VirtualMachine::State::running));

TEST_F(TestDaemonMount, mountFailsErrorMounting)
{
    const auto [temp_dir,
                filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto error = "permission denied";
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(*mock_vm, current_state).WillRepeatedly(Return(mp::VirtualMachine::State::running));
    EXPECT_CALL(*mock_vm, add_mount).WillOnce(Throw(mp::MountActivationError{error}));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    request.set_mount_type(mp::MountRequest::NATIVE);
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("error mounting \"{}\": {}", fake_target_path, error)));
}

TEST_F(TestDaemonMount, performanceMountsNotImplementedHasErrorFails)
{
    const auto [temp_dir,
                filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(*mock_vm, add_mount)
        .WillOnce(Throw(mp::NotImplementedOnThisBackendException("native mounts")));
    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    request.set_mount_type(mp::MountRequest::NATIVE);
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(),
                StrEq("The native mounts feature is not implemented on this backend."));
}

TEST_F(TestDaemonMount, mountUsesResolvedSource)
{
    // can't use _ since _ is needed for gmock matching later.
    const auto [temp_dir, ignored_filename] = plant_instance_json(
        fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    const auto target_path = config_builder.data_directory.toStdString();

    // have resolver return target_path
    const auto [mock_file_ops, file_ops_guard] = mpt::MockFileOps::inject<NiceMock>();
    EXPECT_CALL(*mock_file_ops, exists(A<const std::filesystem::path&>()))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, weakly_canonical).WillOnce(Return(target_path));

    // Check the VMMount passed to the instance uses the resolved source.
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(*mock_vm,
                add_mount(_,
                          Matcher<const mp::VMMount&>(
                              Property(&mp::VMMount::get_source_path, Eq(target_path))),
                          _));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    // setup to make the daemon happy
    MP_DELEGATE_MOCK_CALLS_ON_BASE(*mock_file_ops, setStaleLockTime, FileOps);
    MP_DELEGATE_MOCK_CALLS_ON_BASE(*mock_file_ops, tryLock, FileOps);
    MP_DELEGATE_MOCK_CALLS_ON_BASE(*mock_file_ops, mkpath, FileOps);
    MP_DELEGATE_MOCK_CALLS_ON_BASE(*mock_file_ops, commit, FileOps);
    MP_DELEGATE_MOCK_CALLS_ON_BASE_WITH_MATCHERS(*mock_file_ops,
                                                 open,
                                                 FileOps,
                                                 (A<QIODevice&>(), A<QIODevice::OpenMode>()));

    mp::Daemon daemon{config_builder.build()};

    mp::MountRequest request;
    request.set_source_path(mount_dir.path().toStdString());
    request.set_mount_type(mp::MountRequest::NATIVE);
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);
    entry->set_target_path(fake_target_path);

    // invoke daemon and expect it to be happy
    auto status = call_daemon_slot(
        daemon,
        &mp::Daemon::mount,
        request,
        StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonMount, mountsReportedFromOtherThreadsArePersistedOnTheDaemonThread)
{
    const auto [temp_dir,
                filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    const auto* vm = mock_vm.get();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};
    mp::VMStatusMonitor& monitor = daemon;

    const std::unordered_map<std::string, mp::VMMount> mounts{
        {fake_target_path,
         {mount_dir.path().toStdString(), {}, {}, mp::VMMount::MountType::Native}}};
    std::thread{[&] { monitor.update_mounts_for(*vm, mounts); }}.join();

    check_mounts_in_json(boost::json::parse(mp::utils::contents_of(filename)), {});

    QCoreApplication::processEvents();
    check_mounts_in_json(boost::json::parse(mp::utils::contents_of(filename)), mounts);
}

TEST_F(TestDaemonMount, mountsReportedByStaleInstancesAreIgnored)
{
    const auto [temp_dir,
                filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*mock_vm, get_name).WillRepeatedly(ReturnRef(mock_instance_name));

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

    mp::Daemon daemon{config_builder.build()};
    mp::VMStatusMonitor& monitor = daemon;

    const std::string ghost_name{"ghost"};
    NiceMock<mpt::MockVirtualMachine> impostor, ghost;
    EXPECT_CALL(impostor, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
    EXPECT_CALL(ghost, get_name).WillRepeatedly(ReturnRef(ghost_name));

    const std::unordered_map<std::string, mp::VMMount> mounts{
        {fake_target_path,
         {mount_dir.path().toStdString(), {}, {}, mp::VMMount::MountType::Native}}};
    monitor.update_mounts_for(impostor, mounts);
    monitor.update_mounts_for(ghost, mounts);

    EXPECT_TRUE(monitor.retrieve_mounts_for(mock_instance_name).empty());
    EXPECT_TRUE(monitor.retrieve_mounts_for(ghost_name).empty());
    daemon.persist_instances();
    EXPECT_THAT(mp::utils::contents_of(filename), Not(HasSubstr(ghost_name)));
}

TEST_F(TestDaemonMount, queuedPruningDoesNotUndoLaterUnmount)
{
    const mp::VMMount mount{mount_dir.path().toStdString(), {}, {}, mp::VMMount::MountType::Native};
    const std::unordered_map<std::string, mp::VMMount> initial{{"/good", mount}, {"/bad", mount}};
    const auto [temp_dir, filename] = plant_instance_json(
        fake_json_contents(mac_addr, extra_interfaces, initial));
    config_builder.data_directory = temp_dir->path();

    MountTestVirtualMachine* vm = nullptr;
    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce([&](const auto& desc, const auto& key_provider, mp::VMStatusMonitor& monitor) {
            auto instance = std::make_unique<NiceMock<MountTestVirtualMachine>>(desc.vm_name,
                                                                                desc,
                                                                                monitor,
                                                                                key_provider,
                                                                                zone);
            vm = instance.get();
            MP_DELEGATE_MOCK_CALLS_ON_BASE(*vm, get_name, mp::BaseVirtualMachine);
            EXPECT_CALL(*vm, make_native_mount_handler("/good", _))
                .WillOnce(Return(std::make_unique<mpt::StubMountHandler>()));
            auto bad = std::make_unique<NiceMock<mpt::MockMountHandler>>();
            EXPECT_CALL(*bad, activate_impl).WillOnce(Throw(std::runtime_error{"bad mount"}));
            EXPECT_CALL(*vm, make_native_mount_handler("/bad", _)).WillOnce(Return(std::move(bad)));
            instance->load_mounts();
            return instance;
        });
    mp::Daemon daemon{config_builder.build()};
    mp::VMStatusMonitor& monitor = daemon;

    std::thread{[&] {
        const auto result = vm->activate_mounts(mp::ServerVariant{});
        EXPECT_THAT(result.pruned, ElementsAre(Pair("/bad", "bad mount")));
    }}.join();

    vm->remove_mount("/good");
    EXPECT_THAT(monitor.retrieve_mounts_for(mock_instance_name), IsEmpty());

    QCoreApplication::sendPostedEvents(&daemon, QEvent::MetaCall);
    EXPECT_THAT(monitor.retrieve_mounts_for(mock_instance_name), IsEmpty());
    check_mounts_in_json(boost::json::parse(mp::utils::contents_of(filename)), {});

    EXPECT_CALL(*vm, make_native_mount_handler("/good", _))
        .WillOnce(Return(std::make_unique<mpt::StubMountHandler>()));
    std::thread{[&] { vm->add_mount("/good", mount, mp::ServerVariant{}); }}.join();
    QCoreApplication::sendPostedEvents(&daemon, QEvent::MetaCall);
    check_mounts_in_json(boost::json::parse(mp::utils::contents_of(filename)), {{"/good", mount}});
}

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
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>

#if defined(HYPERV_HCS_ENABLED)
#include "hyperv_api/mock_hyperv_hcn_wrapper.h"
#include "hyperv_api/mock_hyperv_hcs_wrapper.h"

#include <daemon/hyperv_driver_transition.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>
#endif

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
// White-box subclass for migration-state setup.
struct GuardTestDaemon : public mp::Daemon
{
    using mp::Daemon::Daemon;

    void begin_migration()
    {
        migration_in_progress = true;
    }

    bool is_migrating() const
    {
        return migration_in_progress.load();
    }

    void add_instance(const std::string& name, mp::VirtualMachine::ShPtr vm)
    {
        operative_instances.emplace(name, std::move(vm));
        vm_instance_specs.emplace(name, mp::VMSpecs{});
    }
};

struct TestDaemonMigrationGuard : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
        config_builder.server_address = "localhost:0";
    }

    template <typename Reply, typename Request, typename DaemonSlotPtr>
    grpc::Status call_while_migrating(DaemonSlotPtr slot, const Request& request)
    {
        GuardTestDaemon daemon{config_builder.build()};
        daemon.begin_migration();
        // A guarded RPC must reject before doing any work, so it must never write a reply.
        StrictMock<mpt::MockServerReaderWriter<Reply, Request>> server;
        EXPECT_CALL(server, Write(_, _)).Times(0);
        return call_daemon_slot(daemon, slot, request, std::move(server));
    }

    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;
};
} // namespace

TEST_F(TestDaemonMigrationGuard, startRejectedWhileMigrating)
{
    const auto status = call_while_migrating<mp::StartReply>(&mp::Daemon::start,
                                                             mp::StartRequest{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(),
                AllOf(HasSubstr("start instances"), HasSubstr("migration")));
}

TEST_F(TestDaemonMigrationGuard, settingsRejectedWhileMigrating)
{
    const auto status = call_while_migrating<mp::SetReply>(&mp::Daemon::set, mp::SetRequest{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(), HasSubstr("Cannot change settings"));
}

TEST_F(TestDaemonMigrationGuard, versionAllowedWhileMigrating)
{
    GuardTestDaemon daemon{config_builder.build()};
    daemon.begin_migration();

    StrictMock<mpt::MockServerReaderWriter<mp::VersionReply, mp::VersionRequest>> server;
    EXPECT_CALL(server, Write(_, _)).Times(1);

    const auto status = call_daemon_slot(daemon,
                                         &mp::Daemon::version,
                                         mp::VersionRequest{},
                                         server);
    EXPECT_TRUE(status.ok());
}

#if defined(HYPERV_HCS_ENABLED)
TEST_F(TestDaemonMigrationGuard, driverChangeRejectsRunningHcsInstance)
{
    GuardTestDaemon daemon{config_builder.build()};
    auto vm = std::make_shared<NiceMock<mpt::MockVirtualMachine>>();
    ON_CALL(*vm, current_state).WillByDefault(Return(mp::VirtualMachine::State::running));
    daemon.add_instance("running", vm);

    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("hyperv");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::set, request, server);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(), HasSubstr("instance is not stopped"));
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeHoldsGuardAcrossSettingsWrite)
{
    GuardTestDaemon daemon{config_builder.build()};
    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(mock_settings, set(Eq(mp::driver_key), Eq("hyperv"), _)).WillOnce([&daemon] {
        EXPECT_TRUE(daemon.is_migrating());
    });

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("HYPER-V");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::set, request, server).ok());
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeReleasesGuardWhenSettingsWriteFails)
{
    GuardTestDaemon daemon{config_builder.build()};
    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("virtualbox")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(mock_settings, set(Eq(mp::driver_key), Eq("virtualbox"), _)).WillOnce([&daemon] {
        EXPECT_TRUE(daemon.is_migrating());
        throw std::runtime_error{"settings write failed"};
    });

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("virtualbox");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::set, request, server);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), "settings write failed");
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeReleasesHcsResourcesBeforeSettingsWrite)
{
    const auto hcs_mock = mpt::MockHCSWrapper::inject<StrictMock>();
    const auto hcn_mock = mpt::MockHCNWrapper::inject<StrictMock>();
    GuardTestDaemon daemon{config_builder.build()};
    auto vm = std::make_shared<StrictMock<mpt::MockVirtualMachine>>();
    daemon.add_instance("stopped", vm);

    InSequence sequence;
    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(*vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*hcs_mock.first, open_compute_system("stopped", _))
        .WillOnce(Return(mp::hyperv::OperationResult{HCS_E_SYSTEM_NOT_FOUND, L""}));
    EXPECT_CALL(*hcn_mock.first, delete_endpoint(mp::hyperv::endpoint_guid_for_mac("")))
        .WillOnce(Return(mp::hyperv::OperationResult{HCN_E_ENDPOINT_NOT_FOUND, L""}));
    EXPECT_CALL(mock_settings, set(Eq(mp::driver_key), Eq("hyperv"), _)).WillOnce([&daemon] {
        EXPECT_TRUE(daemon.is_migrating());
    });

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("hyperv");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::set, request, server).ok());
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeDoesNotWriteSettingsWhenResourceCleanupFails)
{
    const auto hcs_mock = mpt::MockHCSWrapper::inject<StrictMock>();
    GuardTestDaemon daemon{config_builder.build()};
    auto vm = std::make_shared<StrictMock<mpt::MockVirtualMachine>>();
    daemon.add_instance("stopped", vm);

    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(*vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*hcs_mock.first, open_compute_system("stopped", _))
        .WillOnce(Return(mp::hyperv::OperationResult{E_ACCESSDENIED, L"access denied"}));

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("hyperv");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::set, request, server);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), "Could not release hyperv_api resources for 'stopped'");
    EXPECT_FALSE(daemon.is_migrating());
}

namespace
{
struct TestHyperVDriverTransition : public TestDaemonMigrationGuard
{
    mp::hyperv::DriverTransition transition(const mp::DaemonConfig& config)
    {
        return {config, specs, instances, deleted_instances, migrating, preparing};
    }

    std::unordered_map<std::string, mp::VMSpecs> specs;
    mp::hyperv::DriverTransition::InstanceTable instances;
    mp::hyperv::DriverTransition::InstanceTable deleted_instances;
    std::atomic<bool> migrating{false};
    std::atomic_size_t preparing{0};
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;
};
} // namespace

TEST_F(TestHyperVDriverTransition, unrelatedSettingDoesNotAcquireGuardOrReadDriver)
{
    const auto config = config_builder.build();
    auto change = transition(*config);

    EXPECT_TRUE(change.prepare("local.mounts", "true").ok());
    EXPECT_FALSE(migrating);
    EXPECT_TRUE(change.complete(&server).ok());
}

TEST_F(TestHyperVDriverTransition, unrelatedDriverChangeDoesNotAcquireGuard)
{
    const auto config = config_builder.build();
    auto change = transition(*config);
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("virtualbox")));

    EXPECT_TRUE(change.prepare(mp::driver_key, "hyperv").ok());
    EXPECT_FALSE(migrating);
    EXPECT_TRUE(change.complete(&server).ok());
}

TEST_F(TestHyperVDriverTransition, unchangedHcsDriverDoesNotAcquireGuard)
{
    const auto config = config_builder.build();
    auto change = transition(*config);
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));

    EXPECT_TRUE(change.prepare(mp::driver_key, "hyperv_api").ok());
    EXPECT_FALSE(migrating);
    EXPECT_TRUE(change.complete(&server).ok());
}

TEST_F(TestHyperVDriverTransition, conflictingTransitionDoesNotReleaseAnotherOwnersGuard)
{
    const auto config = config_builder.build();
    migrating = true;
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    {
        auto change = transition(*config);
        const auto status = change.prepare(mp::driver_key, "hyperv");
        EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
        EXPECT_EQ(status.error_message(),
                  mp::hyperv::migration_conflict_status("change settings").error_message());
    }
    EXPECT_TRUE(migrating);
}

TEST_F(TestHyperVDriverTransition, preparationConflictReleasesAcquiredGuard)
{
    const auto config = config_builder.build();
    preparing = 1;
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    {
        auto change = transition(*config);
        const auto status = change.prepare(mp::driver_key, "hyperv");
        EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
        EXPECT_EQ(status.error_message(),
                  "Cannot change driver while an instance is being prepared");
    }
    EXPECT_FALSE(migrating);
    EXPECT_EQ(preparing.load(), 1);
}

TEST_F(TestHyperVDriverTransition, deletedHcsInstancesAreCheckedBeforeReleasingResources)
{
    const auto config = config_builder.build();
    auto vm = std::make_shared<StrictMock<mpt::MockVirtualMachine>>();
    deleted_instances.emplace("deleted", vm);
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(*vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::running));

    MP_EXPECT_THROW_THAT(
        {
            auto change = transition(*config);
            (void)change.prepare(mp::driver_key, "hyperv");
        },
        std::exception,
        Property(&std::exception::what, HasSubstr("instance is not stopped")));
    EXPECT_FALSE(migrating);
}
#endif

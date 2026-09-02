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
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
// White-box subclass: lets tests toggle the migration flag and reach the pure guard helper.
struct GuardTestDaemon : public mp::Daemon
{
    using mp::Daemon::Daemon;
    using mp::Daemon::migration_conflict_status;

    void begin_migration()
    {
        migration_in_progress = true;
    }
};

struct TestDaemonMigrationGuard : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
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

// -------------------------------------------------------------------------------------------------
// Pure guard helper
// -------------------------------------------------------------------------------------------------

TEST_F(TestDaemonMigrationGuard, conflictStatusEmptyWhenNotMigrating)
{
    EXPECT_FALSE(
        GuardTestDaemon::migration_conflict_status(false, "purge instances").has_value());
}

TEST_F(TestDaemonMigrationGuard, conflictStatusRejectsWhenMigrating)
{
    const auto status = GuardTestDaemon::migration_conflict_status(true, "purge instances");
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status->error_message(),
                AllOf(HasSubstr("purge instances"), HasSubstr("migration")));
}

// -------------------------------------------------------------------------------------------------
// Representative mutating RPCs are rejected while a migration is in progress
// -------------------------------------------------------------------------------------------------

TEST_F(TestDaemonMigrationGuard, startRejectedWhileMigrating)
{
    const auto status =
        call_while_migrating<mp::StartReply>(&mp::Daemon::start, mp::StartRequest{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
}

TEST_F(TestDaemonMigrationGuard, suspendRejectedWhileMigrating)
{
    const auto status =
        call_while_migrating<mp::SuspendReply>(&mp::Daemon::suspend, mp::SuspendRequest{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
}

TEST_F(TestDaemonMigrationGuard, restartRejectedWhileMigrating)
{
    const auto status =
        call_while_migrating<mp::RestartReply>(&mp::Daemon::restart, mp::RestartRequest{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
}

TEST_F(TestDaemonMigrationGuard, snapshotRejectedWhileMigrating)
{
    const auto status =
        call_while_migrating<mp::SnapshotReply>(&mp::Daemon::snapshot, mp::SnapshotRequest{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
}

TEST_F(TestDaemonMigrationGuard, restoreRejectedWhileMigrating)
{
    const auto status =
        call_while_migrating<mp::RestoreReply>(&mp::Daemon::restore, mp::RestoreRequest{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
}

// -------------------------------------------------------------------------------------------------
// Read-only RPCs remain available during a migration
// -------------------------------------------------------------------------------------------------

TEST_F(TestDaemonMigrationGuard, versionAllowedWhileMigrating)
{
    GuardTestDaemon daemon{config_builder.build()};
    daemon.begin_migration();

    StrictMock<mpt::MockServerReaderWriter<mp::VersionReply, mp::VersionRequest>> server;
    EXPECT_CALL(server, Write(_, _)).Times(1);

    const auto status =
        call_daemon_slot(daemon, &mp::Daemon::version, mp::VersionRequest{}, server);
    EXPECT_TRUE(status.ok());
}

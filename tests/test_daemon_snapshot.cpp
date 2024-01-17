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
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestDaemonSnapshot : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
    }

    mpt::MockPlatform::GuardedMock mock_platform_injection{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform& mock_platform = *mock_platform_injection.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_F(TestDaemonSnapshot, failsIfBackendDoesNotSupportSnapshots)
{
    auto& mock_factory = *use_a_mock_vm_factory();
    EXPECT_CALL(mock_factory, require_snapshots_support)
        .WillRepeatedly(Throw(mp::NotImplementedOnThisBackendException{"snapshots"}));

    mp::Daemon daemon{config_builder.build()};
    auto status = call_daemon_slot(daemon,
                                   &mp::Daemon::snapshot,
                                   mp::SnapshotRequest{},
                                   StrictMock<mpt::MockServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_THAT(status.error_message(), AllOf(HasSubstr("not implemented"), HasSubstr("snapshots")));
}
} // namespace

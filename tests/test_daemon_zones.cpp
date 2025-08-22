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

#include "blueprint_test_lambdas.h"
#include "daemon_test_fixture.h"
#include "mock_availability_zone.h"
#include "mock_availability_zone_manager.h"
#include "mock_json_utils.h"
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

struct TestDaemonZones : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());

        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

        ON_CALL(*zone1, get_name()).WillByDefault(ReturnRef(zone1_name));
        ON_CALL(*zone2, get_name()).WillByDefault(ReturnRef(zone2_name));

        ON_CALL(*mock_az_manager, get_zone(zone1_name)).WillByDefault(ReturnRef(*zone1.get()));
        ON_CALL(*mock_az_manager, get_zone(zone2_name)).WillByDefault(ReturnRef(*zone2.get()));

        ON_CALL(*mock_az_manager, get_zones())
            .WillByDefault(Return(
                std::vector<std::reference_wrapper<const mp::AvailabilityZone>>{*zone1.get(),
                                                                                *zone2.get()}));
    }

    const std::string zone1_name = "zone1";
    const std::string zone2_name = "zone2";

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockJsonUtils::GuardedMock mock_json_utils_injection =
        mpt::MockJsonUtils::inject<NiceMock>();

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;

    std::unique_ptr<mpt::MockAvailabilityZoneManager> mock_az_manager =
        std::make_unique<NiceMock<mpt::MockAvailabilityZoneManager>>();

    std::unique_ptr<mpt::MockAvailabilityZone> zone1 =
        std::make_unique<NiceMock<mpt::MockAvailabilityZone>>();
    std::unique_ptr<mpt::MockAvailabilityZone> zone2 =
        std::make_unique<NiceMock<mpt::MockAvailabilityZone>>();
};

// multipass disable-zones
TEST_F(TestDaemonZones, zonesStateCmdDisablesAll)
{
    ON_CALL(*zone1, is_available()).WillByDefault(Return(true));
    EXPECT_CALL(*zone1, set_available(false)).Times(1);

    ON_CALL(*zone2, is_available()).WillByDefault(Return(true));
    EXPECT_CALL(*zone2, set_available(false)).Times(1);

    config_builder.az_manager = std::move(mock_az_manager);
    mp::Daemon daemon{config_builder.build()};

    mp::ZonesStateRequest request;
    request.set_available(false);
    StrictMock<mpt::MockServerReaderWriter<mp::ZonesStateReply, mp::ZonesStateRequest>> mock_server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::zones_state, request, mock_server);

    EXPECT_TRUE(status.ok());
}

// multipass zones
TEST_F(TestDaemonZones, zonesCmdReturnsMultipleZones)
{
    EXPECT_CALL(*zone1, get_name()).WillOnce(ReturnRef(zone1_name));
    EXPECT_CALL(*zone1, is_available()).WillOnce(Return(false));

    EXPECT_CALL(*zone2, get_name()).WillOnce(ReturnRef(zone2_name));
    EXPECT_CALL(*zone2, is_available()).WillOnce(Return(true));

    config_builder.az_manager = std::move(mock_az_manager);
    mp::Daemon daemon{config_builder.build()};

    mp::ZonesRequest request;
    StrictMock<mpt::MockServerReaderWriter<mp::ZonesReply, mp::ZonesRequest>> mock_server;

    mp::ZonesReply reply;
    EXPECT_CALL(mock_server, Write(_, _))
        .WillRepeatedly([&reply](const mp::ZonesReply& written_reply, auto) {
            reply = written_reply;
            return true;
        });

    const auto status = call_daemon_slot(daemon, &mp::Daemon::zones, request, mock_server);

    ASSERT_TRUE(status.ok());
    EXPECT_THAT(reply.zones(),
                UnorderedElementsAre(AllOf(Property(&mp::Zone::available, IsFalse()),
                                           Property(&mp::Zone::name, Eq(zone1_name))),
                                     AllOf(Property(&mp::Zone::available, IsTrue()),
                                           Property(&mp::Zone::name, Eq(zone2_name)))));
}

TEST_F(TestDaemonZones, zonesCmdReturnsNoZones)
{
    EXPECT_CALL(*mock_az_manager, get_zones())
        .WillRepeatedly(Return(std::vector<std::reference_wrapper<const mp::AvailabilityZone>>{}));

    config_builder.az_manager = std::move(mock_az_manager);
    mp::Daemon daemon{config_builder.build()};

    mp::ZonesRequest request;
    StrictMock<mpt::MockServerReaderWriter<mp::ZonesReply, mp::ZonesRequest>> mock_server;

    mp::ZonesReply reply;
    EXPECT_CALL(mock_server, Write(_, _))
        .WillRepeatedly([&reply](const mp::ZonesReply& written_reply, auto) {
            reply = written_reply;
            return true;
        });

    const auto status = call_daemon_slot(daemon, &mp::Daemon::zones, request, mock_server);

    ASSERT_TRUE(status.ok());
    EXPECT_THAT(reply.zones(), IsEmpty());
}

TEST_F(TestDaemonZones, zonesCmdFailsOnException)
{
    EXPECT_CALL(*mock_az_manager, get_zones()).WillOnce(Throw(std::runtime_error("test_error")));

    config_builder.az_manager = std::move(mock_az_manager);
    mp::Daemon daemon{config_builder.build()};

    mp::ZonesRequest request;
    StrictMock<mpt::MockServerReaderWriter<mp::ZonesReply, mp::ZonesRequest>> mock_server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::zones, request, mock_server);

    ASSERT_TRUE(!status.ok());
    EXPECT_THAT(status.error_message(), HasSubstr("test_error"));
}

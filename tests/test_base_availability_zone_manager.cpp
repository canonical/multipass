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
#include "mock_file_ops.h"
#include "mock_json_utils.h"
#include "mock_logger.h"

#include <multipass/base_availability_zone_manager.h>
#include <multipass/constants.h>
#include <multipass/exceptions/availability_zone_exceptions.h>

#include <QString>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

struct BaseAvailabilityZoneManagerTest : public Test
{
    BaseAvailabilityZoneManagerTest()
    {
        mock_logger.mock_logger->screen_logs(mpl::Level::error);
    }

    const mp::fs::path data_dir{"/path/to/data"};
    const mp::fs::path manager_file = data_dir / "az-manager.json";
    const mp::fs::path zones_dir = data_dir / "zones";
    const QString manager_file_qstr{QString::fromStdString(manager_file.u8string())};

    mpt::MockFileOps::GuardedMock mock_file_ops_guard{mpt::MockFileOps::inject()};
    mpt::MockJsonUtils::GuardedMock mock_json_utils_guard{mpt::MockJsonUtils::inject()};
    mpt::MockLogger::Scope mock_logger{mpt::MockLogger::inject()};
};

TEST_F(BaseAvailabilityZoneManagerTest, CreatesDefaultZones)
{
    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(manager_file)).WillOnce(Return(QJsonObject{}));

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::trace), _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::debug), _, _)).Times(AnyNumber());

    // Default zones will be created
    const int expected_zone_count = static_cast<int>(mp::default_zone_names.size());
    for (const auto& zone_name : mp::default_zone_names)
    {
        const auto zone_file = zones_dir / (std::string{zone_name} + ".json");
        EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(zone_file)).WillOnce(Return(QJsonObject{}));
        EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(zone_file.u8string())));
    }

    // Manager file gets written with default zone (once in constructor and once in get_automatic_zone_name)
    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, manager_file_qstr)).Times(2);

    mp::BaseAvailabilityZoneManager manager{data_dir};

    const auto zones = manager.get_zones();
    EXPECT_EQ(zones.size(), expected_zone_count);

    // First zone in default_zone_names should be our default
    EXPECT_EQ(manager.get_default_zone_name(), *mp::default_zone_names.begin());
    // And also our automatic zone initially
    EXPECT_EQ(manager.get_automatic_zone_name(), *mp::default_zone_names.begin());
}

TEST_F(BaseAvailabilityZoneManagerTest, AutomaticZoneRotation)
{
    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(manager_file)).WillOnce(Return(QJsonObject{}));

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::trace), _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::debug), _, _)).Times(AnyNumber());

    // Set up all zones to be created
    for (const auto& zone_name : mp::default_zone_names)
    {
        const auto zone_file = zones_dir / (std::string{zone_name} + ".json");
        EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(zone_file)).WillOnce(Return(QJsonObject{}));
        EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(zone_file.u8string())))
            .Times(AnyNumber());
    }

    // Manager file will be written multiple times during rotation
    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, manager_file_qstr)).Times(AnyNumber());

    mp::BaseAvailabilityZoneManager manager{data_dir};

    // First automatic zone should be the first zone
    const auto first_zone = manager.get_automatic_zone_name();
    EXPECT_EQ(first_zone, *mp::default_zone_names.begin());

    // Mark first zone as unavailable
    manager.get_zone(first_zone).set_available(false);

    // Next automatic zone should be second zone
    const auto second_zone = manager.get_automatic_zone_name();
    EXPECT_NE(second_zone, first_zone);

    // Mark all zones unavailable
    for (const auto& zone : manager.get_zones())
        const_cast<mp::AvailabilityZone&>(zone.get()).set_available(false);

    // Expect exception when no zones available
    EXPECT_THROW(manager.get_automatic_zone_name(), mp::NoAvailabilityZoneAvailable);
}

TEST_F(BaseAvailabilityZoneManagerTest, ThrowsWhenZoneNotFound)
{
    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(manager_file)).WillOnce(Return(QJsonObject{}));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    // Set up default zones to be created
    for (const auto& zone_name : mp::default_zone_names)
    {
        const auto zone_file = zones_dir / (std::string{zone_name} + ".json");
        EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(zone_file)).WillOnce(Return(QJsonObject{}));
        EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(zone_file.u8string())))
            .Times(AnyNumber());
    }

    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, manager_file_qstr)).Times(AnyNumber());

    mp::BaseAvailabilityZoneManager manager{data_dir};

    EXPECT_THROW(manager.get_zone("nonexistent-zone"), mp::AvailabilityZoneNotFound);
}

TEST_F(BaseAvailabilityZoneManagerTest, CyclesThroughAvailableZones)
{
    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(manager_file)).WillOnce(Return(QJsonObject{}));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    // Set up default zones to be created - all initially available
    for (const auto& zone_name : mp::default_zone_names)
    {
        const auto zone_file = zones_dir / (std::string{zone_name} + ".json");
        EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(zone_file)).WillOnce(Return(QJsonObject{}));
        EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(zone_file.u8string())))
            .Times(AnyNumber());
    }

    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, manager_file_qstr)).Times(AnyNumber());

    mp::BaseAvailabilityZoneManager manager{data_dir};

    // First call returns initial zone (zone1)
    auto zone = manager.get_automatic_zone_name();
    EXPECT_EQ(zone, "zone1");

    // Make zone2 unavailable
    manager.get_zone("zone2").set_available(false);

    // Second call follows pointer to zone2 (unavailable), so moves to and returns zone3
    zone = manager.get_automatic_zone_name();
    EXPECT_EQ(zone, "zone3");

    // Third call follows pointer to zone1 and returns it
    zone = manager.get_automatic_zone_name();
    EXPECT_EQ(zone, "zone1");

    // Fourth call follows pointer to zone2 (unavailable), so moves to and returns zone3
    zone = manager.get_automatic_zone_name();
    EXPECT_EQ(zone, "zone3");
}

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

#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/stub_availability_zone_manager.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct DefaultStubAvailabilityZoneManagerTest : public Test
{
    mp::StubAvailabilityZoneManager manager;
};

TEST_F(DefaultStubAvailabilityZoneManagerTest, hasNonEmptyDefaultAndAutomaticZoneName)
{
    EXPECT_EQ(manager.get_default_zone_name(), "zone1");
    EXPECT_EQ(manager.get_automatic_zone_name(), "zone1");
}

TEST_F(DefaultStubAvailabilityZoneManagerTest, getZoneRespectsRequestedNameAndIsAlwaysAvailable)
{
    auto& zone = manager.get_zone("zone1");
    EXPECT_EQ(zone.get_name(), "zone1");
    EXPECT_TRUE(zone.is_available());
}

TEST_F(DefaultStubAvailabilityZoneManagerTest, getZoneFailsWithInvalidName)
{
    for (const auto& name : {"", "not-a-zone"})
        EXPECT_THROW(manager.get_zone(name), mp::AvailabilityZoneNotFound);
}

TEST_F(DefaultStubAvailabilityZoneManagerTest, getZonesReturnsTheSingleStubZone)
{
    const auto zones = manager.get_zones();
    ASSERT_EQ(zones.size(), 1u);
    EXPECT_EQ(zones[0].get().get_name(), "zone1");
}

TEST_F(DefaultStubAvailabilityZoneManagerTest, setAvailableIsANoOp)
{
    auto& zone = manager.get_zone("zone1");
    zone.set_available(false);
    EXPECT_TRUE(zone.is_available());
}

struct CustomStubAvailabilityZoneManagerTest : public Test
{
    mp::StubAvailabilityZoneManager manager{{"192.168.0.0/24"}, {"192.168.1.0/24"}, {"192.168.2.0/24"}};
};

TEST_F(CustomStubAvailabilityZoneManagerTest, getZoneRespectsRequestedNameAndIsAlwaysAvailable)
{
    for (const auto& name : {"zone1", "zone2", "zone3"})
    {
        auto& zone = manager.get_zone(name);
        EXPECT_EQ(zone.get_name(), name);
        EXPECT_TRUE(zone.is_available());
    }
}

TEST_F(CustomStubAvailabilityZoneManagerTest, getZonesReturnsTheSingleStubZone)
{
    const auto zones = manager.get_zones();
    ASSERT_EQ(zones.size(), 3u);
    EXPECT_EQ(zones[0].get().get_name(), "zone1");
    EXPECT_EQ(zones[1].get().get_name(), "zone2");
    EXPECT_EQ(zones[2].get().get_name(), "zone3");
}

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
#include "mock_logger.h"
#include "mock_virtual_machine.h"

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/fake_availability_zone_manager.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

struct FakeAvailabilityZoneManagerTest : public Test
{
    FakeAvailabilityZoneManagerTest()
    {
        mock_logger.mock_logger->screen_logs(mpl::Level::error);
    }

    mpt::MockLogger::Scope mock_logger{mpt::MockLogger::inject()};
};

TEST_F(FakeAvailabilityZoneManagerTest, AllowsGetZone1ButThrowsForOtherZones)
{
    mp::FakeAvailabilityZoneManager manager;

    // zone1 should be allowed
    EXPECT_NO_THROW(manager.get_zone("zone1"));

    // Any other zone should throw
    EXPECT_THROW(manager.get_zone("zone2"), mp::NotImplementedOnThisBackendException);
    EXPECT_THROW(manager.get_zone("us-west-1"), mp::NotImplementedOnThisBackendException);
}

TEST_F(FakeAvailabilityZoneManagerTest, ThrowsNotImplementedOnGetZones)
{
    mp::FakeAvailabilityZoneManager manager;

    EXPECT_THROW(manager.get_zones(), mp::NotImplementedOnThisBackendException);
}

TEST_F(FakeAvailabilityZoneManagerTest, AllowsGetAutomaticZoneName)
{
    mp::FakeAvailabilityZoneManager manager;

    EXPECT_EQ(manager.get_automatic_zone_name(), "zone1");
}

TEST_F(FakeAvailabilityZoneManagerTest, AllowsGetDefaultZoneName)
{
    mp::FakeAvailabilityZoneManager manager;

    EXPECT_EQ(manager.get_default_zone_name(), "zone1");
}

TEST_F(FakeAvailabilityZoneManagerTest, ConstructorCreatesZone1)
{
    // This test verifies that the constructor works without throwing
    EXPECT_NO_THROW(mp::FakeAvailabilityZoneManager manager{});
}

TEST_F(FakeAvailabilityZoneManagerTest, ZoneSetAvailableThrowsNotImplemented)
{
    mp::FakeAvailabilityZoneManager manager;
    auto& zone = manager.get_zone("zone1");

    // Setting zone availability should throw NotImplementedOnThisBackendException
    EXPECT_THROW(zone.set_available(false), mp::NotImplementedOnThisBackendException);
    EXPECT_THROW(zone.set_available(true), mp::NotImplementedOnThisBackendException);
}

TEST_F(FakeAvailabilityZoneManagerTest, ZoneGetNameReturnsCorrectName)
{
    mp::FakeAvailabilityZoneManager manager;
    auto& zone = manager.get_zone("zone1");

    EXPECT_EQ(zone.get_name(), "zone1");
}

TEST_F(FakeAvailabilityZoneManagerTest, ZoneGetSubnetReturnsSubnet)
{
    mp::FakeAvailabilityZoneManager manager;
    auto& zone = manager.get_zone("zone1");

    EXPECT_EQ(zone.get_subnet(), "10.0.0.0/24");
}

TEST_F(FakeAvailabilityZoneManagerTest, ZoneIsAvailableReturnsTrue)
{
    mp::FakeAvailabilityZoneManager manager;
    auto& zone = manager.get_zone("zone1");

    // The fake zone should always report as available
    EXPECT_TRUE(zone.is_available());
}

TEST_F(FakeAvailabilityZoneManagerTest, ZoneAddVmDoesNotThrow)
{
    mp::FakeAvailabilityZoneManager manager;
    auto& zone = manager.get_zone("zone1");

    // Create a mock VM for testing
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>("test-vm");

    // add_vm should be a no-op and not throw
    EXPECT_NO_THROW(zone.add_vm(*mock_vm));
}

TEST_F(FakeAvailabilityZoneManagerTest, ZoneRemoveVmDoesNotThrow)
{
    mp::FakeAvailabilityZoneManager manager;
    auto& zone = manager.get_zone("zone1");

    // Create a mock VM for testing
    auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>("test-vm");

    // remove_vm should be a no-op and not throw
    EXPECT_NO_THROW(zone.remove_vm(*mock_vm));
}

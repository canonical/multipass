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
#include "mock_subnet_utils.h"
#include "mock_virtual_machine.h"

#include <multipass/base_availability_zone.h>
#include <multipass/constants.h>

#include <QString>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

struct BaseAvailabilityZoneTest : public Test
{
    BaseAvailabilityZoneTest()
    {
        mock_logger.mock_logger->screen_logs(mpl::Level::error);
    }

    const std::string az_name{"zone1"};
    const mp::fs::path az_dir{"/path/to/zones"};
    const mp::fs::path az_file = az_dir / (az_name + ".json");
    const QString az_file_qstr{QString::fromStdU16String(az_file.u16string())};
    const mp::Subnet az_subnet{"192.168.1.0/24"};

    mpt::MockFileOps::GuardedMock mock_file_ops_guard{mpt::MockFileOps::inject()};
    mpt::MockJsonUtils::GuardedMock mock_json_utils_guard{mpt::MockJsonUtils::inject()};
    mpt::MockLogger::Scope mock_logger{mpt::MockLogger::inject()};
    mpt::MockSubnetUtils::GuardedMock mock_subnet_utils_guard{mpt::MockSubnetUtils::inject()};
};

TEST_F(BaseAvailabilityZoneTest, CreatesDefaultAvailableZone)
{
    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file))
        .WillOnce(Return(QJsonObject{}));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_json_utils_guard.first,
                write_json(_, QString::fromStdU16String(az_file.u16string())));

    EXPECT_CALL(*mock_subnet_utils_guard.first, random_subnet_from_range(_, _))
        .WillOnce(Return(az_subnet));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    EXPECT_EQ(zone.get_name(), az_name);
    EXPECT_TRUE(zone.is_available());
}

TEST_F(BaseAvailabilityZoneTest, loads_existing_zone_file)
{
    const mp::Subnet test_subnet{"10.0.0.0/24"};
    const bool test_available = false;

    QJsonObject json{{"subnet", QString::fromStdString(test_subnet.to_cidr())},
                     {"available", test_available}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file))
        .WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_json_utils_guard.first,
                write_json(_, QString::fromStdU16String(az_file.u16string())));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    EXPECT_EQ(zone.get_name(), az_name);
    EXPECT_EQ(zone.get_subnet(), test_subnet);
    EXPECT_FALSE(zone.is_available());
}

TEST_F(BaseAvailabilityZoneTest, AddsVmAndUpdatesOnAvailabilityChange)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file))
        .WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_json_utils_guard.first,
                write_json(_, QString::fromStdU16String(az_file.u16string())))
        .Times(2); // Once in constructor, once in set_available

    EXPECT_CALL(*mock_subnet_utils_guard.first, random_subnet_from_range(_, _))
        .WillOnce(Return(az_subnet));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    const std::string test_vm_name = "test-vm";
    NiceMock<mpt::MockVirtualMachine> mock_vm{test_vm_name};
    EXPECT_CALL(mock_vm, set_available(false));

    zone.add_vm(mock_vm);
    zone.set_available(false);
}

TEST_F(BaseAvailabilityZoneTest, RemovesVmCorrectly)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file))
        .WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_json_utils_guard.first,
                write_json(_, QString::fromStdU16String(az_file.u16string())));

    EXPECT_CALL(*mock_subnet_utils_guard.first, random_subnet_from_range(_, _))
        .WillOnce(Return(az_subnet));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    const std::string test_vm_name = "test-vm";
    NiceMock<mpt::MockVirtualMachine> mock_vm{test_vm_name};

    zone.add_vm(mock_vm);
    zone.remove_vm(mock_vm);
}

TEST_F(BaseAvailabilityZoneTest, AvailabilityStateManagement)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file))
        .WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_json_utils_guard.first,
                write_json(_, QString::fromStdU16String(az_file.u16string())))
        .Times(2); // Once in constructor, once in set_available

    EXPECT_CALL(*mock_subnet_utils_guard.first, random_subnet_from_range(_, _))
        .WillOnce(Return(az_subnet));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    const std::string vm1_name = "test-vm1";
    const std::string vm2_name = "test-vm2";
    NiceMock<mpt::MockVirtualMachine> mock_vm1{vm1_name};
    NiceMock<mpt::MockVirtualMachine> mock_vm2{vm2_name};

    // Both VMs should be notified when state changes
    EXPECT_CALL(mock_vm1, set_available(false));
    EXPECT_CALL(mock_vm2, set_available(false));

    zone.add_vm(mock_vm1);
    zone.add_vm(mock_vm2);

    // Setting to current state (true) shouldn't trigger VM updates
    zone.set_available(true);

    // Setting to new state should notify all VMs
    zone.set_available(false);
}

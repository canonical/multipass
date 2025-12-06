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
#include "mock_logger.h"
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
    mpt::MockLogger::Scope mock_logger{mpt::MockLogger::inject()};
};

TEST_F(BaseAvailabilityZoneTest, CreatesDefaultAvailableZone)
{
    EXPECT_CALL(*mock_file_ops_guard.first, open_read(az_file, _))
        .WillOnce(Return(mpt::mock_read_data("{}")));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_file_ops_guard.first,
                write_transactionally(QString::fromStdU16String(az_file.u16string()), _));

    mp::BaseAvailabilityZone zone{az_name, 0, az_dir};

    EXPECT_EQ(zone.get_name(), az_name);
    EXPECT_TRUE(zone.is_available());
}

TEST_F(BaseAvailabilityZoneTest, loads_existing_zone_file)
{
    const mp::Subnet test_subnet{"10.0.0.0/24"};

    const auto json = "{\"subnet\": \"10.0.0.0/24\", \"available\": false}";
    EXPECT_CALL(*mock_file_ops_guard.first, open_read(az_file, _))
        .WillOnce(Return(mpt::mock_read_data(json)));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_file_ops_guard.first,
                write_transactionally(QString::fromStdU16String(az_file.u16string()), _));

    mp::BaseAvailabilityZone zone{az_name, 0, az_dir};

    EXPECT_EQ(zone.get_name(), az_name);
    EXPECT_EQ(zone.get_subnet(), test_subnet);
    EXPECT_FALSE(zone.is_available());
}

TEST_F(BaseAvailabilityZoneTest, AddsVmAndUpdatesOnAvailabilityChange)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_file_ops_guard.first, open_read(az_file, _))
        .WillOnce(Return(mpt::mock_read_data("{}")));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_file_ops_guard.first,
                write_transactionally(QString::fromStdU16String(az_file.u16string()), _))
        .Times(2); // Once in constructor, once in set_available

    mp::BaseAvailabilityZone zone{az_name, 0, az_dir};

    NiceMock<mpt::MockVirtualMachine> mock_vm;
    EXPECT_CALL(mock_vm, set_available(false));

    zone.add_vm(mock_vm);
    zone.set_available(false);
}

TEST_F(BaseAvailabilityZoneTest, RemovesVmCorrectly)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_file_ops_guard.first, open_read(az_file, _))
        .WillOnce(Return(mpt::mock_read_data("{}")));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_file_ops_guard.first,
                write_transactionally(QString::fromStdU16String(az_file.u16string()), _));

    mp::BaseAvailabilityZone zone{az_name, 0, az_dir};

    NiceMock<mpt::MockVirtualMachine> mock_vm;

    zone.add_vm(mock_vm);
    zone.remove_vm(mock_vm);
}

TEST_F(BaseAvailabilityZoneTest, AvailabilityStateManagement)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_file_ops_guard.first, open_read(az_file, _))
        .WillOnce(Return(mpt::mock_read_data("{}")));

    EXPECT_CALL(*mock_logger.mock_logger, log(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock_file_ops_guard.first,
                write_transactionally(QString::fromStdU16String(az_file.u16string()), _))
        .Times(2); // Once in constructor, once in set_available

    mp::BaseAvailabilityZone zone{az_name, 0, az_dir};

    NiceMock<mpt::MockVirtualMachine> mock_vm1;
    NiceMock<mpt::MockVirtualMachine> mock_vm2;

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

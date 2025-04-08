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
#include "mock_virtual_machine.h"

#include <multipass/base_availability_zone.h>
#include <multipass/constants.h>

#include <QString>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

struct TestBaseAvailabilityZone : public Test
{
    TestBaseAvailabilityZone() : mock_logger{mpt::MockLogger::inject()}
    {
        mock_logger.mock_logger->screen_logs(mpl::Level::error);
    }

    const std::string az_name{"zone1"};
    const mp::fs::path az_dir{"/path/to/zones"};
    const mp::fs::path az_file = az_dir / (az_name + ".json");
    const QString az_file_qstr{QString::fromStdString(az_file.u8string())};

    mpt::MockFileOps::GuardedMock mock_file_ops_guard{mpt::MockFileOps::inject()};
    mpt::MockJsonUtils::GuardedMock mock_json_utils_guard{mpt::MockJsonUtils::inject()};
    mpt::MockLogger::Scope mock_logger;
};

TEST_F(TestBaseAvailabilityZone, creates_default_available_zone)
{
    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file)).WillOnce(Return(QJsonObject{}));

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::debug), _, _))
        .Times(2); // Once for subnet missing, once for availability missing
    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::trace), _, _))
        .Times(2); // Once in constructor, once in serialize()

    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(az_file.u8string())));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    EXPECT_EQ(zone.get_name(), az_name);
    EXPECT_TRUE(zone.is_available());
    EXPECT_TRUE(zone.get_subnet().empty());
}

TEST_F(TestBaseAvailabilityZone, loads_existing_zone_file)
{
    const std::string test_subnet = "10.0.0.0/24";
    const bool test_available = false;

    QJsonObject json{{"subnet", QString::fromStdString(test_subnet)}, {"available", test_available}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file)).WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::trace), _, _))
        .Times(2); // Once in constructor, once in serialize()

    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(az_file.u8string())));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    EXPECT_EQ(zone.get_name(), az_name);
    EXPECT_EQ(zone.get_subnet(), test_subnet);
    EXPECT_FALSE(zone.is_available());
}

TEST_F(TestBaseAvailabilityZone, adds_vm_and_updates_on_availability_change)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file)).WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::trace), _, _))
        .Times(3); // Once in constructor, once in serialize(), once in set_available

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::debug), _, _))
        .Times(3); // Once for subnet missing, once for adding VM, once for availability change

    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(az_file.u8string())))
        .Times(2); // Once in constructor, once in set_available

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    const std::string test_vm_name = "test-vm";
    NiceMock<mpt::MockVirtualMachine> mock_vm{test_vm_name};
    EXPECT_CALL(mock_vm, make_available(false));

    zone.add_vm(mock_vm);
    zone.set_available(false);
}

TEST_F(TestBaseAvailabilityZone, removes_vm_correctly)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file)).WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::trace), _, _))
        .Times(2); // Once in constructor, once in serialize()

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::debug), _, _))
        .Times(3); // Once for subnet missing, once for adding VM, once for removing VM

    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(az_file.u8string())));

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    const std::string test_vm_name = "test-vm";
    NiceMock<mpt::MockVirtualMachine> mock_vm{test_vm_name};

    zone.add_vm(mock_vm);
    zone.remove_vm(mock_vm);
}

TEST_F(TestBaseAvailabilityZone, availability_state_management)
{
    QJsonObject json{{"available", true}};

    EXPECT_CALL(*mock_json_utils_guard.first, read_object_from_file(az_file)).WillOnce(Return(json));

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::trace), _, _))
        .Times(3); // Once in constructor, once in serialize(), once in set_available(false)

    EXPECT_CALL(*mock_logger.mock_logger, log(Eq(mpl::Level::debug), _, _))
        .Times(5); // Once for subnet, once per VM add, once for no-op set_available, once for actual change

    EXPECT_CALL(*mock_json_utils_guard.first, write_json(_, QString::fromStdString(az_file.u8string())))
        .Times(2); // Once in constructor, once in set_available

    mp::BaseAvailabilityZone zone{az_name, az_dir};

    const std::string vm1_name = "test-vm1";
    const std::string vm2_name = "test-vm2";
    NiceMock<mpt::MockVirtualMachine> mock_vm1{vm1_name};
    NiceMock<mpt::MockVirtualMachine> mock_vm2{vm2_name};

    // Both VMs should be notified when state changes
    EXPECT_CALL(mock_vm1, make_available(false));
    EXPECT_CALL(mock_vm2, make_available(false));

    zone.add_vm(mock_vm1);
    zone.add_vm(mock_vm2);

    // Setting to current state (true) shouldn't trigger VM updates
    zone.set_available(true);

    // Setting to new state should notify all VMs
    zone.set_available(false);
}

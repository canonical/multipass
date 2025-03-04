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
#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/format.h>

#include <filesystem>
#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace fs = std::filesystem;
using namespace testing;

namespace
{
struct TestBaseAvailabilityZone : public Test
{
    const std::string test_name{"test-zone"};
    const fs::path test_dir{"/test/path"};
    const fs::path test_file{test_dir / (test_name + ".json")};
    mpt::MockLogger::Scope mock_logger{mpt::MockLogger::inject()};
    mpt::MockFileOps::GuardedMock mock_file_ops{mpt::MockFileOps::inject()};

    TestBaseAvailabilityZone()
    {
        auto& mock_file = *mock_file_ops.first;
        EXPECT_CALL(mock_file, status(_, _))
            .WillRepeatedly(
                DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::not_found})));

        EXPECT_CALL(mock_file, open_write(_, _)).WillRepeatedly(Invoke([](const auto&, const auto&) {
            return std::make_unique<std::stringstream>();
        }));

        mock_logger.mock_logger->screen_logs(mp::logging::Level::error);
    }

    ~TestBaseAvailabilityZone() override = default;
};

struct TestBaseAvailabilityZoneWithVM : public TestBaseAvailabilityZone
{
    TestBaseAvailabilityZoneWithVM()
    {
        zone = std::make_unique<mp::BaseAvailabilityZone>(test_name, test_dir);
        mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>("test-vm", *zone);
        zone->add_vm(*mock_vm);
    }

    std::unique_ptr<mp::BaseAvailabilityZone> zone;
    std::unique_ptr<NiceMock<mpt::MockVirtualMachine>> mock_vm;
};

} // namespace

TEST_F(TestBaseAvailabilityZone, creates_with_defaults_when_file_not_found)
{
    mock_logger.mock_logger->expect_log(mp::logging::Level::info, "creating zone");
    mock_logger.mock_logger->expect_log(mp::logging::Level::info, "not found");

    mp::BaseAvailabilityZone zone{test_name, test_dir};

    EXPECT_TRUE(zone.is_available());
    EXPECT_EQ(zone.get_name(), test_name);
    EXPECT_TRUE(zone.get_subnet().empty());
}

TEST_F(TestBaseAvailabilityZone, throws_on_inaccessible_file)
{
    const std::error_code test_error{EACCES, std::system_category()};
    EXPECT_CALL(*mock_file_ops.first, status(test_file, _))
        .WillOnce(DoAll(SetArgReferee<1>(test_error), Return(fs::file_status{fs::file_type::regular})));

    EXPECT_THROW(mp::BaseAvailabilityZone(test_name, test_dir), mp::AvailabilityZoneDeserializationError);
}

TEST_F(TestBaseAvailabilityZone, throws_on_invalid_file_type)
{
    EXPECT_CALL(*mock_file_ops.first, status(test_file, _))
        .WillOnce(DoAll(SetArgReferee<1>(std::error_code{}), Return(fs::file_status{fs::file_type::directory})));

    EXPECT_THROW(mp::BaseAvailabilityZone(test_name, test_dir), mp::AvailabilityZoneDeserializationError);
}

TEST_F(TestBaseAvailabilityZone, logs_availability_changes)
{
    mp::BaseAvailabilityZone zone{test_name, test_dir};

    mock_logger.mock_logger->expect_log(mp::logging::Level::info, "making AZ unavailable");

    zone.set_available(false);
    EXPECT_FALSE(zone.is_available());

    mock_logger.mock_logger->expect_log(mp::logging::Level::info, "making AZ available");

    zone.set_available(true);
    EXPECT_TRUE(zone.is_available());
}

TEST_F(TestBaseAvailabilityZoneWithVM, propagates_availability_to_vms)
{
    EXPECT_CALL(*mock_vm, make_available(false));
    zone->set_available(false);

    EXPECT_CALL(*mock_vm, make_available(true));
    zone->set_available(true);
}

TEST_F(TestBaseAvailabilityZoneWithVM, removes_vm_by_name)
{
    auto other_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>("other-vm", *zone);
    auto& other_vm_ref = *other_vm;
    zone->add_vm(*other_vm);

    EXPECT_CALL(*mock_vm, make_available(_)).Times(0);
    EXPECT_CALL(other_vm_ref, make_available(false));

    zone->remove_vm(*mock_vm);
    zone->set_available(false);
}

TEST_F(TestBaseAvailabilityZone, throws_on_serialization_failure)
{
    EXPECT_CALL(*mock_file_ops.first, open_write(test_file, _)).WillOnce(Invoke([](const auto&, const auto&) {
        auto stream = std::make_unique<std::stringstream>();
        stream->setstate(std::ios::failbit);
        return stream;
    }));

    EXPECT_THROW(mp::BaseAvailabilityZone(test_name, test_dir), mp::AvailabilityZoneSerializationError);
}

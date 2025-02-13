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

#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/format.h>

#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

namespace
{
// Exception test helper
template <typename Ex>
void test_exception_what(const char* message)
{
    try
    {
        throw Ex(message);
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_EQ(e.what(), std::string(message));
    }
}
} // namespace

TEST(AvailabilityZoneExceptionsTest, formats_availability_zone_error_message)
{
    const std::string expected = "test error message";
    test_exception_what<mp::AvailabilityZoneError>(expected.c_str());
}

TEST(AvailabilityZoneExceptionsTest, formats_availability_zone_serialization_error)
{
    const std::string expected = "test serialization error";
    test_exception_what<mp::AvailabilityZoneSerializationError>(expected.c_str());
}

TEST(AvailabilityZoneExceptionsTest, formats_availability_zone_deserialization_error)
{
    const std::string expected = "test deserialization error";
    test_exception_what<mp::AvailabilityZoneDeserializationError>(expected.c_str());
}

TEST(AvailabilityZoneExceptionsTest, formats_availability_zone_manager_error_message)
{
    const std::string expected = "test manager error";
    test_exception_what<mp::AvailabilityZoneManagerError>(expected.c_str());
}

TEST(AvailabilityZoneExceptionsTest, formats_availability_zone_manager_serialization_error)
{
    const std::string expected = "test manager serialization error";
    test_exception_what<mp::AvailabilityZoneManagerSerializationError>(expected.c_str());
}

TEST(AvailabilityZoneExceptionsTest, formats_availability_zone_manager_deserialization_error)
{
    const std::string expected = "test manager deserialization error";
    test_exception_what<mp::AvailabilityZoneManagerDeserializationError>(expected.c_str());
}

TEST(AvailabilityZoneExceptionsTest, formats_availability_zone_not_found_error)
{
    const std::string zone_name = "test-zone";
    mp::AvailabilityZoneNotFound ex{zone_name};
    const std::string expected = fmt::format("no AZ with name {:?} found", zone_name);
    EXPECT_EQ(ex.what(), expected);
}

TEST(AvailabilityZoneExceptionsTest, formats_no_availability_zone_available_error)
{
    mp::NoAvailabilityZoneAvailable ex{};
    EXPECT_EQ(ex.what(), std::string("no AZ is available"));
}

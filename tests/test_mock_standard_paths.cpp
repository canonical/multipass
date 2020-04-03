/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include "mock_standard_paths.h"

#include <multipass/standard_paths.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{

TEST(StandardPaths, provides_regular_locate_by_default)
{
    // TODO@ricab
}

TEST(StandardPaths, can_have_locate_mocked)
{
    const auto& mock = mpt::MockStandardPaths::mock_instance();
    const auto location_type = mp::StandardPaths::HomeLocation;
    const auto locate_options = mp::StandardPaths::LocateOptions{mp::StandardPaths::LocateFile};
    const auto find = QStringLiteral("abc");
    const auto proof = QStringLiteral("xyz");

    EXPECT_CALL(mock, locate(location_type, find, locate_options)).WillOnce(Return(proof));
    ASSERT_EQ(mp::StandardPaths::instance().locate(location_type, find, locate_options), proof);
}

TEST(StandardPaths, provides_regular_standard_locations_by_default)
{
    // TODO@ricab
}

TEST(StandardPaths, can_have_standard_locations_mocked)
{
    const auto& mock = mpt::MockStandardPaths::mock_instance();
    const auto test = mp::StandardPaths::AppConfigLocation;
    const auto proof = QStringList{QStringLiteral("abc"), QStringLiteral("xyz")};

    EXPECT_CALL(mock, standardLocations(test)).WillOnce(Return(proof));
    ASSERT_EQ(mp::StandardPaths::instance().standardLocations(test), proof);
}

TEST(StandardPaths, provides_regular_writable_location_by_default)
{
    // TODO@ricab
}

TEST(StandardPaths, can_have_writable_location_mocked)
{
    const auto& mock = mpt::MockStandardPaths::mock_instance();
    const auto test = mp::StandardPaths::ConfigLocation;
    const auto proof = QStringLiteral("xyz");

    EXPECT_CALL(mock, writableLocation(test)).WillOnce(Return(proof));
    ASSERT_EQ(mp::StandardPaths::instance().writableLocation(test), proof);
}
} // namespace

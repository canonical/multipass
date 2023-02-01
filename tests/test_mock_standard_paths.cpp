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
#include "mock_standard_paths.h"

#include <multipass/standard_paths.h>

#include <QStandardPaths>
#include <QTemporaryDir>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{

TEST(StandardPaths, provides_regular_locate_by_default)
{
    const auto location_type = mp::StandardPaths::TempLocation;
    const auto find = QStringLiteral("o_o");
    const auto locate_options = mp::StandardPaths::LocateOptions{mp::StandardPaths::LocateDirectory};

    // Create a subdir in the standard temp dir, for locate to find
    QDir aux{QStandardPaths::writableLocation(location_type)};
    ASSERT_TRUE(aux.exists());
    aux.mkdir(find);
    ASSERT_TRUE(aux.exists(find));

    // Confirm the subdir is properly located
    const auto proof = QStandardPaths::locate(location_type, find, locate_options);
    ASSERT_TRUE(proof.endsWith(find));
    ASSERT_EQ(MP_STDPATHS.locate(location_type, find, locate_options), proof);
}

TEST(StandardPaths, can_have_locate_mocked)
{
    const auto location_type = mp::StandardPaths::HomeLocation;
    const auto locate_options = mp::StandardPaths::LocateOptions{mp::StandardPaths::LocateFile};
    const auto find = QStringLiteral("abc");
    const auto proof = QStringLiteral("xyz");
    auto& mock = mpt::MockStandardPaths::mock_instance();

    EXPECT_CALL(mock, locate(location_type, find, locate_options)).WillOnce(Return(proof));
    ASSERT_EQ(MP_STDPATHS.locate(location_type, find, locate_options), proof);
}

TEST(StandardPaths, provides_regular_standard_locations_by_default)
{
    const auto& test = mp::StandardPaths::MusicLocation;
    ASSERT_EQ(MP_STDPATHS.standardLocations(test), QStandardPaths::standardLocations(test));
}

TEST(StandardPaths, can_have_standard_locations_mocked)
{
    const auto test = mp::StandardPaths::AppConfigLocation;
    const auto proof = QStringList{QStringLiteral("abc"), QStringLiteral("xyz")};
    auto& mock = mpt::MockStandardPaths::mock_instance();

    EXPECT_CALL(mock, standardLocations(test)).WillOnce(Return(proof));
    ASSERT_EQ(MP_STDPATHS.standardLocations(test), proof);
}

TEST(StandardPaths, provides_regular_writable_location_by_default)
{
    const auto test = mp::StandardPaths::MoviesLocation;
    ASSERT_EQ(MP_STDPATHS.writableLocation(test), QStandardPaths::writableLocation(test));
}

TEST(StandardPaths, can_have_writable_location_mocked)
{
    const auto test = mp::StandardPaths::ConfigLocation;
    const auto proof = QStringLiteral("xyz");
    auto& mock = mpt::MockStandardPaths::mock_instance();

    EXPECT_CALL(mock, writableLocation(test)).WillOnce(Return(proof));
    ASSERT_EQ(MP_STDPATHS.writableLocation(test), proof);
}
} // namespace

/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <tests/common.h>
#include <tests/mock_platform.h>
#include <tests/temp_file.h>

#include "mock_libc_functions.h"

#include <multipass/format.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestPlatformUnix : public Test
{
    mpt::TempFile file;
};
} // namespace

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNotRestrictedIsCorrect)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        .WillOnce(Return(0));

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), false));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsRestrictedIsCorrect)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    const int gid{42};
    struct group group;
    group.gr_gid = gid;

    EXPECT_CALL(*mock_platform, chown(_, 0, gid)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)).WillOnce(Return(0));

    REPLACE(getgrnam, [&group](auto) { return &group; });

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), true));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNoUnixPathThrows)
{
    MP_EXPECT_THROW_THAT(MP_PLATFORM.set_server_socket_restrictions(file.name().toStdString(), false),
                         std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("invalid server address specified: {}", file.name()))));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsNonUnixTypeReturns)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown).Times(0);
    EXPECT_CALL(*mock_platform, chmod).Times(0);

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("dns:{}", file.name()), false));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsChownFailsThrows)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce([](auto...) {
        errno = EPERM;
        return -1;
    });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), false),
        std::runtime_error,
        mpt::match_what(StrEq("Could not set ownership of the multipass socket: Operation not permitted")));
}

TEST_F(TestPlatformUnix, setServerSocketRestrictionsChmodFailsThrows)
{
    auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        .WillOnce([](auto...) {
            errno = EPERM;
            return -1;
        });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_socket_restrictions(fmt::format("unix:{}", file.name()), false),
        std::runtime_error,
        mpt::match_what(StrEq("Could not set permissions for the multipass socket: Operation not permitted")));
}

TEST_F(TestPlatformUnix, chmodSetsFileModsAndReturns)
{
    auto perms = QFileInfo(file.name()).permissions();
    ASSERT_EQ(perms, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);

    EXPECT_EQ(MP_PLATFORM.chmod(file.name().toStdString().c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), 0);

    perms = QFileInfo(file.name()).permissions();

    EXPECT_EQ(perms, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser |
                         QFileDevice::ReadGroup | QFileDevice::WriteGroup);
}

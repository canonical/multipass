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

    // MockPlatform here is needed to mock chown/chmod in set_server_permissions()
    // May need to move this out if other PlatformUnix functions are to be tested
    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject()};
    mpt::MockPlatform* mock_platform = attr.first;
};
} // namespace

TEST_F(TestPlatformUnix, setServerPermissionsNotRestrictedIsCorrect)
{
    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        .WillOnce(Return(0));

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_permissions(fmt::format("unix:{}", file.name()), false));
}

TEST_F(TestPlatformUnix, setServerPermissionsRestrictedIsCorrect)
{
    const int gid{42};
    struct group group;
    group.gr_gid = gid;

    EXPECT_CALL(*mock_platform, chown(_, 0, gid)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)).WillOnce(Return(0));

    REPLACE(getgrnam, [&group](auto) { return &group; });

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_permissions(fmt::format("unix:{}", file.name()), true));
}

TEST_F(TestPlatformUnix, setServerPermissionsNoUnixPathThrows)
{
    MP_EXPECT_THROW_THAT(MP_PLATFORM.Platform::set_server_permissions(file.name().toStdString(), false),
                         std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("invalid server address specified: {}", file.name()))));
}

TEST_F(TestPlatformUnix, setServerPermissionsNonUnixTypeReturns)
{
    EXPECT_CALL(*mock_platform, chown).Times(0);
    EXPECT_CALL(*mock_platform, chmod).Times(0);

    EXPECT_NO_THROW(MP_PLATFORM.Platform::set_server_permissions(fmt::format("dns:{}", file.name()), false));
}

TEST_F(TestPlatformUnix, setServerPermissionsChownFailsThrows)
{
    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce([](auto...) {
        errno = EPERM;
        return -1;
    });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_permissions(fmt::format("unix:{}", file.name()), false), std::runtime_error,
        mpt::match_what(StrEq("Could not set ownership of the multipass socket: Operation not permitted")));
}

TEST_F(TestPlatformUnix, setServerPermissionsChmodFailsThrows)
{
    EXPECT_CALL(*mock_platform, chown(_, 0, 0)).WillOnce(Return(0));
    EXPECT_CALL(*mock_platform, chmod(_, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
        .WillOnce([](auto...) {
            errno = EPERM;
            return -1;
        });

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.Platform::set_server_permissions(fmt::format("unix:{}", file.name()), false), std::runtime_error,
        mpt::match_what(StrEq("Could not set permissions for the multipass socket: Operation not permitted")));
}

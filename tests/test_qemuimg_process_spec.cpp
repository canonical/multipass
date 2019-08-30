/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <src/platform/backends/shared/linux/qemuimg_process_spec.h>

#include "mock_environment_helpers.h"
#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

TEST(TestQemuImgProcessSpec, program_correct)
{
    mp::QemuImgProcessSpec spec({});

    EXPECT_EQ(spec.program(), "qemu-img");
}

TEST(TestQemuImgProcessSpec, default_arguments_correct)
{
    mp::QemuImgProcessSpec spec({});

    EXPECT_EQ(spec.arguments(), QStringList());
}

TEST(TestQemuImgProcessSpec, arguments_set_correctly)
{
    QStringList args{"-one", "--two"};
    mp::QemuImgProcessSpec spec(args);

    EXPECT_EQ(spec.arguments(), args);
}

TEST(TestQemuImgProcessSpec, apparmor_profile_has_correct_name)
{
    mp::QemuImgProcessSpec spec({});

    EXPECT_TRUE(spec.apparmor_profile().contains("profile multipass.qemu-img"));
}

TEST(TestQemuImgProcessSpec, no_apparmor_profile_identifier)
{
    mp::QemuImgProcessSpec spec({});

    EXPECT_EQ(spec.identifier(), "");
}

TEST(TestQemuImgProcessSpec, apparmor_profile_running_as_snap_correct)
{
    mpt::SetEnvScope e("SNAP", "/something");
    mpt::SetEnvScope e2("SNAP_COMMON", "/snap/common");
    mp::QemuImgProcessSpec spec({});

    EXPECT_TRUE(spec.apparmor_profile().contains("/something/usr/bin/qemu-img ixr,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/snap/common/** rwk,"));
}

TEST(TestQemuImgProcessSpec, apparmor_profile_not_running_as_snap_correct)
{
    mpt::UnsetEnvScope e("SNAP");
    mp::QemuImgProcessSpec spec({});

    EXPECT_TRUE(spec.apparmor_profile().contains("capability dac_read_search,"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/bin/qemu-img ixr,")); // space wanted
}

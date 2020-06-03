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

#include <src/platform/backends/shared/qemuimg_process_spec.h>

#include "tests/mock_environment_helpers.h"
#include <gmock/gmock.h>

#include <QFile>
#include <QTemporaryDir>

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
    QTemporaryDir snap_dir, common_dir;
    mpt::SetEnvScope e("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_COMMON", common_dir.path().toUtf8());
    mp::QemuImgProcessSpec spec({});

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-img ixr,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/** rwk,").arg(common_dir.path())));
}

TEST(TestQemuImgProcessSpec, apparmor_profile_running_as_symlinked_snap_correct)
{
    QTemporaryDir snap_dir, snap_link_dir, common_dir, common_link_dir;
    snap_link_dir.remove();
    common_link_dir.remove();
    QFile::link(snap_dir.path(), snap_link_dir.path());
    QFile::link(common_dir.path(), common_link_dir.path());

    mpt::SetEnvScope e("SNAP", snap_link_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_COMMON", common_link_dir.path().toUtf8());
    mp::QemuImgProcessSpec spec({});

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-img ixr,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/** rwk,").arg(common_dir.path())));
}

TEST(TestQemuImgProcessSpec, apparmor_profile_not_running_as_snap_correct)
{
    mpt::UnsetEnvScope e("SNAP");
    mp::QemuImgProcessSpec spec({});

    EXPECT_TRUE(spec.apparmor_profile().contains("capability dac_read_search,"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/bin/qemu-img ixr,")); // space wanted
}

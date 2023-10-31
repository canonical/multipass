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
#include "disabling_macros.h"
#include "mock_environment_helpers.h"

#include <multipass/process/qemuimg_process_spec.h>

#include <QFile>
#include <QTemporaryDir>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

TEST(TestQemuImgProcessSpec, program_correct)
{
    mp::QemuImgProcessSpec spec({}, "");

    EXPECT_EQ(spec.program(), "qemu-img");
}

TEST(TestQemuImgProcessSpec, default_arguments_correct)
{
    mp::QemuImgProcessSpec spec({}, "");

    EXPECT_EQ(spec.arguments(), QStringList());
}

TEST(TestQemuImgProcessSpec, arguments_set_correctly)
{
    QStringList args{"-one", "--two"};
    mp::QemuImgProcessSpec spec(args, "");

    EXPECT_EQ(spec.arguments(), args);
}

TEST(TestQemuImgProcessSpec, apparmor_profile_has_correct_name)
{
    mp::QemuImgProcessSpec spec({}, "");

    EXPECT_TRUE(spec.apparmor_profile().contains("profile multipass.qemu-img"));
}

TEST(TestQemuImgProcessSpec, no_apparmor_profile_identifier)
{
    mp::QemuImgProcessSpec spec({}, "");

    EXPECT_EQ(spec.identifier(), "");
}

TEST(TestQemuImgProcessSpec, apparmor_profile_running_as_snap_correct)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir;
    QString source_image{"/source/image/file"};

    mpt::SetEnvScope e("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuImgProcessSpec spec({}, source_image);

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-img ixr,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1 rwk,").arg(source_image)));
}

TEST(TestQemuImgProcessSpec, apparmor_profile_running_as_snap_with_target_correct)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir;
    QString source_image{"/source/image/file"}, target_image{"/target/image/file"};

    mpt::SetEnvScope e("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuImgProcessSpec spec({}, source_image, target_image);

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-img ixr,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1 rwk,").arg(source_image)));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1 rwk,").arg(target_image)));
}

TEST(TestQemuImgProcessSpec, apparmor_profile_running_as_snap_with_only_target_correct)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir;
    QString target_image{"/target/image/file"};

    mpt::SetEnvScope e("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuImgProcessSpec spec({}, "", target_image);

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-img ixr,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1 rwk,").arg(target_image)));
}

TEST(TestQemuImgProcessSpec,
     DISABLE_ON_WINDOWS(apparmor_profile_running_as_symlinked_snap_correct)) // TODO tests involving apparmor should
                                                                             // probably be moved elsewhere
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir, snap_link_dir, common_dir, common_link_dir;
    QString source_image{"/source/image/file"};

    snap_link_dir.remove();
    common_link_dir.remove();
    QFile::link(snap_dir.path(), snap_link_dir.path());
    QFile::link(common_dir.path(), common_link_dir.path());

    mpt::SetEnvScope e("SNAP", snap_link_dir.path().toUtf8());
    mpt::SetEnvScope e3("SNAP_NAME", snap_name);
    mp::QemuImgProcessSpec spec({}, source_image);

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-img ixr,").arg(snap_dir.path())));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1 rwk,").arg(source_image)));
}

TEST(TestQemuImgProcessSpec, apparmor_profile_not_running_as_snap_correct)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope e("SNAP");
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuImgProcessSpec spec({}, "");

    EXPECT_TRUE(spec.apparmor_profile().contains("capability dac_read_search,"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/bin/qemu-img ixr,")); // space wanted
}

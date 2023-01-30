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
#include "mock_environment_helpers.h"
#include "temp_dir.h"

#include <src/platform/backends/shared/sshfs_server_process_spec.h>

#include <multipass/sshfs_server_config.h>

#include <QCoreApplication>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestSSHFSServerProcessSpec : public Test
{
    mp::SSHFSServerConfig config{"host",
                                 42,
                                 "username",
                                 "instance",
                                 "private_key",
                                 "source_path",
                                 "target_path",
                                 {{1, 2}, {3, 4}},
                                 {{5, -1}, {6, 10}}};
};

TEST_F(TestSSHFSServerProcessSpec, program_correct)
{
    mp::SSHFSServerProcessSpec spec(config);
    EXPECT_TRUE(spec.program().endsWith("sshfs_server"));
}

TEST_F(TestSSHFSServerProcessSpec, arguments_correct)
{
    mp::SSHFSServerProcessSpec spec(config);
    ASSERT_EQ(spec.arguments().size(), 8);
    EXPECT_EQ(spec.arguments()[0], "host");
    EXPECT_EQ(spec.arguments()[1], "42");
    EXPECT_EQ(spec.arguments()[2], "username");
    EXPECT_EQ(spec.arguments()[3], "source_path");
    EXPECT_EQ(spec.arguments()[4], "target_path");
    // Ordering of the next 2 options not guaranteed, hence the or-s.
    EXPECT_TRUE(spec.arguments()[5] == "6:10,5:-1," || spec.arguments()[5] == "5:-1,6:10,");
    EXPECT_TRUE(spec.arguments()[6] == "3:4,1:2," || spec.arguments()[6] == "1:2,3:4,");
    EXPECT_EQ(spec.arguments()[7], "0");
}

TEST_F(TestSSHFSServerProcessSpec, environment_correct)
{
    mp::SSHFSServerProcessSpec spec(config);

    ASSERT_TRUE(spec.environment().contains("KEY"));
    EXPECT_EQ(spec.environment().value("KEY"), "private_key");
}

TEST_F(TestSSHFSServerProcessSpec, snap_confined_apparmor_profile_returns_expected_data)
{
    mpt::TempDir bin_dir;
    const QByteArray snap_name{"multipass"};

    mpt::SetEnvScope env_scope("SNAP", bin_dir.path().toUtf8());
    mpt::SetEnvScope env_scope2("SNAP_NAME", snap_name);
    mp::SSHFSServerProcessSpec spec(config);

    const auto apparmor_profile = spec.apparmor_profile();

    EXPECT_TRUE(apparmor_profile.contains(bin_dir.path() + "/bin/sshfs_server"));
    EXPECT_TRUE(apparmor_profile.contains(bin_dir.path() + "/{usr/,}lib/**"));
    EXPECT_TRUE(apparmor_profile.contains("signal (receive) peer=snap.multipass.multipassd"));
}

TEST_F(TestSSHFSServerProcessSpec, unconfined_apparmor_profile_returns_expected_data)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope env_scope("SNAP");
    mpt::SetEnvScope env_scope2("SNAP_NAME", snap_name);
    mp::SSHFSServerProcessSpec spec(config);
    QDir current_dir(QCoreApplication::applicationDirPath());
    const auto apparmor_profile = spec.apparmor_profile();

    current_dir.cdUp();

    EXPECT_TRUE(apparmor_profile.contains(current_dir.absolutePath() + "/bin/sshfs_server"));
    EXPECT_TRUE(apparmor_profile.contains(current_dir.absolutePath() + "/{usr/,}lib/**"));
    EXPECT_TRUE(apparmor_profile.contains("signal (receive) peer=unconfined"));
}

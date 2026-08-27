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
    mp::SSHFSServerConfig config{{"username", "private_key", 42, "host", {}},
                                 "instance",
                                 "source_path",
                                 "target_path",
                                 {{1, 2}, {3, 4}},
                                 {{5, -1}, {6, 10}}};

    mp::SSHFSServerConfig make_config(const mp::VSOCKHost& vsock_host) const
    {
        mp::SSHFSServerConfig cfg = config;
        cfg.ssh_coordinates.vsock_host = vsock_host;
        return cfg;
    }
};

TEST_F(TestSSHFSServerProcessSpec, programCorrect)
{
    mp::SSHFSServerProcessSpec spec(config);
    EXPECT_TRUE(spec.program().endsWith("sshfs_server"));
}

TEST_F(TestSSHFSServerProcessSpec, argumentsCorrect)
{
    mp::SSHFSServerProcessSpec spec(config);
    ASSERT_EQ(spec.arguments().size(), 10);
    EXPECT_EQ(spec.arguments()[0], "host");
    EXPECT_EQ(spec.arguments()[1], "42");
    EXPECT_EQ(spec.arguments()[2], "username");
    EXPECT_EQ(spec.arguments()[3], "0");
    EXPECT_EQ(spec.arguments()[4], "NONE");
    EXPECT_EQ(spec.arguments()[5], "source_path");
    EXPECT_EQ(spec.arguments()[6], "target_path");
    // Ordering of the next 2 options not guaranteed, hence the or-s.
    EXPECT_TRUE(spec.arguments()[7] == "6:10,5:-1," || spec.arguments()[7] == "5:-1,6:10,");
    EXPECT_TRUE(spec.arguments()[8] == "3:4,1:2," || spec.arguments()[8] == "1:2,3:4,");
    EXPECT_EQ(spec.arguments()[9], "0");
}

TEST_F(TestSSHFSServerProcessSpec, noneVsockArgumentsCorrect)
{
    mp::SSHFSServerProcessSpec spec(make_config(std::monostate{}));

    EXPECT_EQ(spec.arguments()[3], QString::number(mp::VSOCKTAG_NONE));
    EXPECT_EQ(spec.arguments()[4], "NONE");
}

TEST_F(TestSSHFSServerProcessSpec, hvsockVsockArgumentsCorrect)
{
    mp::SSHFSServerProcessSpec spec(make_config(mp::HVSOCKData{"vm-id-123"}));

    EXPECT_EQ(spec.arguments()[3], QString::number(mp::VSOCKTAG_HVSOCK));
    EXPECT_EQ(spec.arguments()[4], "vm-id-123");
}

TEST_F(TestSSHFSServerProcessSpec, vsockVsockArgumentsCorrect)
{
    mp::SSHFSServerProcessSpec spec(make_config(mp::VSOCKData{7}));

    EXPECT_EQ(spec.arguments()[3], QString::number(mp::VSOCKTAG_VSOCK));
    EXPECT_EQ(spec.arguments()[4], "7");
}

TEST_F(TestSSHFSServerProcessSpec, usockVsockArgumentsCorrect)
{
    mp::SSHFSServerProcessSpec spec(make_config(mp::USOCKData{"/run/socket path"}));

    EXPECT_EQ(spec.arguments()[3], QString::number(mp::VSOCKTAG_USOCK));
    EXPECT_EQ(spec.arguments()[4], "/run/socket path");
}

TEST_F(TestSSHFSServerProcessSpec, differentVsockValuesGenerateDifferentArguments)
{
    const std::vector<mp::VSOCKHost> vsock_hosts{std::monostate{},
                                                 mp::HVSOCKData{"vm-id-123"},
                                                 mp::VSOCKData{7},
                                                 mp::USOCKData{"/run/socket path"}};

    std::vector<std::pair<QString, QString>> vsock_arguments;
    for (const auto& vsock_host : vsock_hosts)
    {
        mp::SSHFSServerProcessSpec spec(make_config(vsock_host));
        const auto args = spec.arguments();
        vsock_arguments.emplace_back(args[3], args[4]);
    }

    // Each VSOCK value must map to a distinct (tag, data) argument pair.
    for (auto i = vsock_arguments.cbegin(); i != vsock_arguments.cend(); ++i)
        for (auto j = std::next(i); j != vsock_arguments.cend(); ++j)
            EXPECT_NE(*i, *j);
}

TEST_F(TestSSHFSServerProcessSpec, sameVsockTypeDifferentValuesGenerateDifferentArguments)
{
    mp::SSHFSServerProcessSpec spec1(make_config(mp::VSOCKData{7}));
    mp::SSHFSServerProcessSpec spec2(make_config(mp::VSOCKData{8}));

    EXPECT_EQ(spec1.arguments()[3], spec2.arguments()[3]); // same tag
    EXPECT_NE(spec1.arguments()[4], spec2.arguments()[4]); // different data
}

TEST_F(TestSSHFSServerProcessSpec, environmentCorrect)
{
    mp::SSHFSServerProcessSpec spec(config);

    ASSERT_TRUE(spec.environment().contains("KEY"));
    EXPECT_EQ(spec.environment().value("KEY"), "private_key");
}

TEST_F(TestSSHFSServerProcessSpec, snapConfinedApparmorProfileReturnsExpectedData)
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

TEST_F(TestSSHFSServerProcessSpec, unconfinedApparmorProfileReturnsExpectedData)
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

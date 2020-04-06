/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include <src/platform/backends/qemu/dnsmasq_process_spec.h>

#include "tests/mock_environment_helpers.h"
#include <gmock/gmock.h>
#include <multipass/ip_address.h>

#include <QFile>
#include <QTemporaryDir>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDnsmasqProcessSpec : public Test
{
    const QString data_dir{"/data"};
    const QString bridge_name{"bridgey"};
    const QString pid_file_path{"/path/to/file.pid"};
    const std::string subnet{"1.2.3"};
};

TEST_F(TestDnsmasqProcessSpec, default_arguments_correct)
{
    mpt::SetEnvScope e1("SNAP", "/something");
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, pid_file_path, subnet);
    EXPECT_EQ(spec.arguments(),
              QStringList({"--strict-order", "--bind-interfaces", "--pid-file=/path/to/file.pid", "--domain=multipass",
                           "--local=/multipass/", "--except-interface=lo", "--interface=bridgey",
                           "--listen-address=1.2.3.1", "--dhcp-no-override", "--dhcp-authoritative",
                           "--dhcp-leasefile=/data/dnsmasq.leases", "--dhcp-hostsfile=/data/dnsmasq.hosts",
                           "--dhcp-range", "1.2.3.2,1.2.3.254,infinite", "--conf-file=/data/dnsmasq.conf"}));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_has_correct_name)
{
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, pid_file_path, subnet);

    EXPECT_TRUE(spec.apparmor_profile().contains("profile multipass.dnsmasq"));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_permits_data_dirs)
{
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, pid_file_path, subnet);

    EXPECT_TRUE(spec.apparmor_profile().contains("/data/dnsmasq.leases rw,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/data/dnsmasq.hosts r,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/path/to/file.pid w,"));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_identifier)
{
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, pid_file_path, subnet);

    EXPECT_EQ(spec.identifier(), "");
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_running_as_snap_correct)
{
    QTemporaryDir snap_dir;

    mpt::SetEnvScope e1("SNAP", snap_dir.path().toUtf8());
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, pid_file_path, subnet);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=snap.multipass.multipassd"));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/sbin/dnsmasq ixr,").arg(snap_dir.path())));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_running_as_symlinked_snap_correct)
{
    QTemporaryDir snap_dir, link_dir;
    link_dir.remove();
    QFile::link(snap_dir.path(), link_dir.path());

    mpt::SetEnvScope e1("SNAP", link_dir.path().toUtf8());
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, pid_file_path, subnet);

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/usr/sbin/dnsmasq ixr,").arg(snap_dir.path())));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_not_running_as_snap_correct)
{
    mpt::UnsetEnvScope e("SNAP");
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, pid_file_path, subnet);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=unconfined"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/sbin/dnsmasq ixr,")); // space wanted
}

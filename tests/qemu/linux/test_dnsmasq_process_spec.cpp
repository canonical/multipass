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

#include "tests/common.h"
#include "tests/mock_environment_helpers.h"

#include <src/platform/backends/qemu/linux/dnsmasq_process_spec.h>

#include <multipass/ip_address.h>

#include <QFile>
#include <QTemporaryDir>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDnsmasqProcessSpec : public Test
{
    const QString data_dir{"/data"};
    const mp::SubnetList subnets{{"bridgey", "1.2.3"}};
    const QString conf_file_path{"/path/to/file.conf"};
};

TEST_F(TestDnsmasqProcessSpec, defaultArgumentsCorrect)
{
    const QByteArray snap_name{"multipass"};

    mpt::SetEnvScope e1("SNAP", "/something");
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::DNSMasqProcessSpec spec(data_dir, subnets, conf_file_path);
    EXPECT_EQ(spec.arguments(),
              QStringList({"--keep-in-foreground",
                           "--strict-order",
                           "--bind-interfaces",
                           "--pid-file",
                           "--domain=multipass",
                           "--local=/multipass/",
                           "--except-interface=lo",
                           "--dhcp-no-override",
                           "--dhcp-ignore-clid",
                           "--dhcp-authoritative",
                           "--dhcp-leasefile=/data/dnsmasq.leases",
                           "--dhcp-hostsfile=/data/dnsmasq.hosts",
                           "--conf-file=/path/to/file.conf",
                           "--interface=bridgey",
                           "--listen-address=1.2.3.1",
                           "--dhcp-range",
                           "1.2.3.2,1.2.3.254,infinite"}));
}

TEST_F(TestDnsmasqProcessSpec, apparmorProfileHasCorrectName)
{
    mp::DNSMasqProcessSpec spec(data_dir, subnets, conf_file_path);

    EXPECT_TRUE(spec.apparmor_profile().contains("profile multipass.dnsmasq"));
}

TEST_F(TestDnsmasqProcessSpec, apparmorProfilePermitsDataDirs)
{
    mp::DNSMasqProcessSpec spec(data_dir, subnets, conf_file_path);

    EXPECT_TRUE(spec.apparmor_profile().contains("/data/dnsmasq.leases rw,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/data/dnsmasq.hosts r,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/path/to/file.conf r,"));
}

TEST_F(TestDnsmasqProcessSpec, apparmorProfileIdentifier)
{
    mp::DNSMasqProcessSpec spec(data_dir, subnets, conf_file_path);

    EXPECT_EQ(spec.identifier(), "");
}

TEST_F(TestDnsmasqProcessSpec, apparmorProfileRunningAsSnapCorrect)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir;

    mpt::SetEnvScope e1("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::DNSMasqProcessSpec spec(data_dir, subnets, conf_file_path);

    EXPECT_TRUE(
        spec.apparmor_profile().contains("signal (receive) peer=snap.multipass.multipassd"));
    EXPECT_TRUE(
        spec.apparmor_profile().contains(QString("%1/usr/sbin/dnsmasq ixr,").arg(snap_dir.path())));
}

TEST_F(TestDnsmasqProcessSpec, apparmorProfileRunningAsSymlinkedSnapCorrect)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir, link_dir;

    link_dir.remove();
    QFile::link(snap_dir.path(), link_dir.path());

    mpt::SetEnvScope e1("SNAP", link_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::DNSMasqProcessSpec spec(data_dir, subnets, conf_file_path);

    EXPECT_TRUE(
        spec.apparmor_profile().contains(QString("%1/usr/sbin/dnsmasq ixr,").arg(snap_dir.path())));
}

TEST_F(TestDnsmasqProcessSpec, apparmorProfileNotRunningAsSnapCorrect)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope e("SNAP");
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::DNSMasqProcessSpec spec(data_dir, subnets, conf_file_path);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=unconfined"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/sbin/dnsmasq ixr,")); // space wanted
}

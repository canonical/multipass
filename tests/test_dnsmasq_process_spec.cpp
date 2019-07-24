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

#include <src/platform/backends/qemu/dnsmasq_process_spec.h>

#include "mock_environment_helpers.h"
#include <gmock/gmock.h>
#include <multipass/ip_address.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDnsmasqProcessSpec : public Test
{
    const QString data_dir{"/data"};
    const QString bridge_name{"bridgey"};
    const mp::IPAddress bridge_range{"1.1.1.1"}, ip_start{"1.2.3.4"}, ip_end{"5.6.7.8"};
};

TEST_F(TestDnsmasqProcessSpec, default_arguments_correct_when_snap)
{
    mpt::SetEnvScope e1("SNAP", "/something");
    mpt::SetEnvScope e2("SNAP_COMMON", "/snap/common");
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, bridge_range, ip_start, ip_end);

    EXPECT_EQ(spec.arguments(),
              QStringList({"--keep-in-foreground", "--pid-file=/snap/common/dnsmasq.pid", "--strict-order",
                           "--bind-interfaces", "--domain=multipass", "--local=/multipass/", "--except-interface=lo",
                           "--interface=bridgey", "--listen-address=1.1.1.1", "--dhcp-no-override",
                           "--dhcp-authoritative", "--dhcp-leasefile=/data/dnsmasq.leases",
                           "--dhcp-hostsfile=/data/dnsmasq.hosts", "--dhcp-range", "1.2.3.4,5.6.7.8,infinite"}));
}

TEST_F(TestDnsmasqProcessSpec, default_arguments_correct_when_not_snap)
{
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, bridge_range, ip_start, ip_end);

    EXPECT_EQ(
        spec.arguments(),
        QStringList({"--keep-in-foreground", "", "--strict-order", "--bind-interfaces", "--domain=multipass",
                     "--local=/multipass/", "--except-interface=lo", "--interface=bridgey", "--listen-address=1.1.1.1",
                     "--dhcp-no-override", "--dhcp-authoritative", "--dhcp-leasefile=/data/dnsmasq.leases",
                     "--dhcp-hostsfile=/data/dnsmasq.hosts", "--dhcp-range", "1.2.3.4,5.6.7.8,infinite"}));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_has_correct_name)
{
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, bridge_range, ip_start, ip_end);

    EXPECT_TRUE(spec.apparmor_profile().contains("profile multipass.dnsmasq"));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_permits_data_dirs)
{
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, bridge_range, ip_start, ip_end);

    EXPECT_TRUE(spec.apparmor_profile().contains("/data/dnsmasq.leases rw,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/data/dnsmasq.hosts r,"));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_identifier)
{
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, bridge_range, ip_start, ip_end);

    EXPECT_EQ(spec.identifier(), "");
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_running_as_snap_correct)
{
    mpt::SetEnvScope e1("SNAP", "/something");
    mpt::SetEnvScope e2("SNAP_COMMON", "/snap/common");
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, bridge_range, ip_start, ip_end);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=snap.multipass.multipassd"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/snap/common/dnsmasq.pid w,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/something/usr/sbin/dnsmasq ixr,"));
}

TEST_F(TestDnsmasqProcessSpec, apparmor_profile_not_running_as_snap_correct)
{
    mpt::UnsetEnvScope e("SNAP");
    mp::DNSMasqProcessSpec spec(data_dir, bridge_name, bridge_range, ip_start, ip_end);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=unconfined"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/{,var/}run/*dnsmasq*.pid w,"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/sbin/dnsmasq ixr,")); // space wanted
}

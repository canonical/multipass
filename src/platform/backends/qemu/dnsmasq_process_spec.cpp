/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "dnsmasq_process_spec.h"
#include "snap_utils.h"

namespace mp = multipass;
namespace ms = multipass::snap;

namespace
{
static QString pid_file()
{
    if (ms::is_snap_confined())
    {
        return QString("%1/dnsmasq.pid").arg(ms::snap_common_dir());
    }
    else
    {
        return QString();
    }
}
} // namespace

mp::DNSMasqProcessSpec::DNSMasqProcessSpec(const QDir& data_dir, const QString& bridge_name,
                                           const mp::IPAddress& bridge_addr, const mp::IPAddress& start_ip,
                                           const mp::IPAddress& end_ip)
    : data_dir(data_dir),
      bridge_name(bridge_name),
      pid_file{::pid_file()},
      bridge_addr(bridge_addr),
      start_ip(start_ip),
      end_ip(end_ip)
{
}

QString mp::DNSMasqProcessSpec::program() const
{
    return QStringLiteral("dnsmasq"); // depend on desired binary being in $PATH
}

QStringList mp::DNSMasqProcessSpec::arguments() const
{
    QString pid;
    if (!pid_file.isNull())
    {
        pid = QString("--pid-file=%1").arg(pid_file);
    }

    return QStringList() << "--keep-in-foreground" << pid << "--strict-order"
                         << "--bind-interfaces"
                         << "--except-interface=lo" << QString("--interface=%1").arg(bridge_name)
                         << QString("--listen-address=%1").arg(QString::fromStdString(bridge_addr.as_string()))
                         << "--dhcp-no-override"
                         << "--dhcp-authoritative"
                         << QString("--dhcp-leasefile=%1").arg(data_dir.filePath("dnsmasq.leases"))
                         << QString("--dhcp-hostsfile=%1").arg(data_dir.filePath("dnsmasq.hosts")) << "--dhcp-range"
                         << QString("%1,%2,infinite")
                                .arg(QString::fromStdString(start_ip.as_string()))
                                .arg(QString::fromStdString(end_ip.as_string()));
}

QString mp::DNSMasqProcessSpec::apparmor_profile() const
{
    // Profile based on https://github.com/Rafiot/apparmor-profiles/blob/master/profiles/usr.sbin.dnsmasq
    QString profile_template(R"END(
#include <tunables/global>
profile %1 flags=(attach_disconnected) {
    #include <abstractions/base>
    #include <abstractions/nameservice>

    capability chown,
    capability net_bind_service,
    capability setgid,
    capability setuid,
    capability dac_override,
    capability dac_read_search,
    capability net_admin,         # for DHCP server
    capability net_raw,           # for DHCP server ping checks
    network inet raw,
    network inet6 raw,

    # Allow multipassd send dnsmasq signals
    signal (receive) peer=%2,

    # access to iface mtu needed for Router Advertisement messages in IPv6
    # Neighbor Discovery protocol (RFC 2461)
    @{PROC}/sys/net/ipv6/conf/*/mtu r,

    # binary and its libs
    %3/usr/sbin/dnsmasq ixr,
    %3/{usr/,}lib/** rm,

    %4 rw,           # Leases file
    %5 r,            # Hosts file

    %6 w,            # pid file
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir;    // root directory: either "/" or $SNAP
    QString signal_peer; // who can send kill signal to dnsmasq

    if (ms::is_snap_confined()) // if snap confined, specify only multipassd can kill dnsmasq
    {
        root_dir = ms::snap_dir();
        signal_peer = "snap.multipass.multipassd";
    }
    else
    {
        signal_peer = "unconfined";
    }

    // If multipassd not confined, we let dnsmasq decide where to create its pid file, but we still need to tell
    // apparmor where that'll be
    QString pid = pid_file;
    if (pid.isNull())
    {
        pid = "/{,var/}run/*dnsmasq*.pid";
    }

    return profile_template.arg(apparmor_profile_name(), signal_peer, root_dir, data_dir.filePath("dnsmasq.leases"),
                                data_dir.filePath("dnsmasq.hosts"), pid);
}

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

#include "dnsmasq_process_spec.h"

#include <multipass/snap_utils.h>

namespace mp = multipass;
namespace mu = multipass::utils;

namespace
{
static QString pid_file()
{
    if (mu::is_snap())
    {
        return QString(mu::snap_common_dir() + "/dnsmasq.pid");
    }
    else
    {
        return QString();
    }
}
} // namespace

mp::DNSMasqProcessSpec::DNSMasqProcessSpec(const mp::Path& data_dir, const QString& bridge_name,
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
    return "dnsmasq"; // depend on desired binary being in $PATH
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
                         << "--domain=multipass"
                         << "--local=/multipass/"
                         << "--except-interface=lo" << QString("--interface=%1").arg(bridge_name)
                         << QString("--listen-address=%1").arg(QString::fromStdString(bridge_addr.as_string()))
                         << "--dhcp-no-override"
                         << "--dhcp-authoritative" << QString("--dhcp-leasefile=%1/dnsmasq.leases").arg(data_dir)
                         << QString("--dhcp-hostsfile=%1/dnsmasq.hosts").arg(data_dir) << "--dhcp-range"
                         << QString("%1,%2,infinite")
                                .arg(QString::fromStdString(start_ip.as_string()))
                                .arg(QString::fromStdString(end_ip.as_string()));
}

mp::logging::Level mp::DNSMasqProcessSpec::error_log_level() const
{
    // dnsmasq only complains if something really wrong
    return mp::logging::Level::error;
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
  %3/usr/sbin/%4 ixr,
  %3/{usr/,}lib/@{multiarch}/{,**/}*.so* rm,

  # CLASSIC ONLY: need to specify required libs from core snap
  /snap/core/*/{,usr/}lib/@{multiarch}/{,**/}*.so* rm,

  %5/dnsmasq.leases rw,           # Leases file
  %5/dnsmasq.hosts r,             # Hosts file

  %6 w,     # pid file
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir;    // root directory: either "" or $SNAP
    QString signal_peer; // who can send kill signal to dnsmasq

    if (mu::is_snap()) // if snap confined, specify only multipassd can kill dnsmasq
    {
        root_dir = mu::snap_dir();
        signal_peer = "snap.multipass.multipassd";
    }
    else
    {
        signal_peer = "unconfined";
    }

    /* Need to supply either the pid file we specified, or the default dnsmasq creates */
    QString pid;
    if (pid_file.isNull())
    {
        pid = "/{,var/}run/*dnsmasq*.pid";
    }
    else
    {
        pid = pid_file;
    }

    return profile_template.arg(apparmor_profile_name(), signal_peer, root_dir, program(), data_dir, pid);
}

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

#include "dnsmasq_process_spec.h"

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/format.h>
#include <multipass/snap_utils.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

mp::DNSMasqProcessSpec::DNSMasqProcessSpec(const mp::Path& data_dir, const QString& bridge_name,
                                           const std::string& subnet, const QString& conf_file_path)
    : data_dir(data_dir), bridge_name(bridge_name), subnet{subnet}, conf_file_path{conf_file_path}
{
}

QString mp::DNSMasqProcessSpec::program() const
{
    return "dnsmasq"; // depend on desired binary being in $PATH
}

QStringList mp::DNSMasqProcessSpec::arguments() const
{
    const auto bridge_addr = mp::IPAddress{fmt::format("{}.1", subnet)};
    const auto start_ip = mp::IPAddress{fmt::format("{}.2", subnet)};
    const auto end_ip = mp::IPAddress{fmt::format("{}.254", subnet)};

    return QStringList() << "--keep-in-foreground"
                         << "--strict-order"
                         << "--bind-interfaces" << QString("--pid-file") << "--domain=multipass"
                         << "--local=/multipass/"
                         << "--except-interface=lo" << QString("--interface=%1").arg(bridge_name)
                         << QString("--listen-address=%1").arg(QString::fromStdString(bridge_addr.as_string()))
                         << "--dhcp-no-override"
                         << "--dhcp-ignore-clid"
                         << "--dhcp-authoritative" << QString("--dhcp-leasefile=%1/dnsmasq.leases").arg(data_dir)
                         << QString("--dhcp-hostsfile=%1/dnsmasq.hosts").arg(data_dir) << "--dhcp-range"
                         << QString("%1,%2,infinite")
                                .arg(QString::fromStdString(start_ip.as_string()))
                                .arg(QString::fromStdString(end_ip.as_string()))
                         // This is to prevent it trying to read /etc/dnsmasq.conf
                         << QString("--conf-file=%1").arg(conf_file_path);
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
  /{,var/lib/snapd/}snap/core18/*/{,usr/}lib/@{multiarch}/{,**/}*.so* rm,

  %5/dnsmasq.leases rw,           # Leases file
  %5/dnsmasq.hosts r,             # Hosts file

  %6 r,     # conf file
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir;    // root directory: either "" or $SNAP
    QString signal_peer; // who can send kill signal to dnsmasq

    try
    {
        // if snap confined, specify only multipassd can kill dnsmasq
        root_dir = mpu::snap_dir();
        signal_peer = "snap.multipass.multipassd";
    }
    catch (const mp::SnapEnvironmentException&)
    {
        signal_peer = "unconfined";
    }

    return profile_template.arg(apparmor_profile_name(), signal_peer, root_dir, program(), data_dir, conf_file_path);
}

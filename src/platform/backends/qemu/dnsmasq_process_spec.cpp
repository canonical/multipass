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

namespace mp = multipass;

mp::DNSMasqProcessSpec::DNSMasqProcessSpec(const QDir& data_dir, const QString& bridge_name,
                                           const mp::IPAddress& bridge_addr, const mp::IPAddress& start_ip,
                                           const mp::IPAddress& end_ip)
    : data_dir(data_dir),
      bridge_name(bridge_name),
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
    return QStringList() << "--keep-in-foreground" << "--strict-order"
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

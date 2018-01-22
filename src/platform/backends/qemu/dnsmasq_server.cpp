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

#include "dnsmasq_server.h"

#include <QFile>

namespace mp = multipass;

namespace
{
auto start_dnsmasq_process(const QDir& data_dir, const mp::IPAddress& bridge_addr, const mp::IPAddress& start_ip,
                           const mp::IPAddress& end_ip)
{
    auto cmd = std::make_unique<QProcess>();
    cmd->start("dnsmasq",
               QStringList() << "--keep-in-foreground"
                             << "--strict-order"
                             << "--bind-interfaces"
                             << "--except-interface=lo"
                             << "--interface=mpbr0"
                             << QString("--listen-address=%1").arg(QString::fromStdString(bridge_addr.as_string()))
                             << "--dhcp-no-override"
                             << "--dhcp-authoritative"
                             << QString("--dhcp-leasefile=%1").arg(data_dir.filePath("dnsmasq.leases"))
                             << QString("--dhcp-hostsfile=%1").arg(data_dir.filePath("dnsmasq.hosts")) << "--dhcp-range"
                             << QString("%1,%2,infinite")
                                    .arg(QString::fromStdString(start_ip.as_string()))
                                    .arg(QString::fromStdString(end_ip.as_string())));

    cmd->waitForStarted();
    return cmd;
}
}

mp::DNSMasqServer::DNSMasqServer(const Path& path, const IPAddress& bridge_addr, const IPAddress& start,
                                 const IPAddress& end)
    : start_ip{start},
      end_ip{end},
      data_dir{QDir(path + "/vm-ips")},
      dnsmasq_cmd{start_dnsmasq_process(data_dir, bridge_addr, start_ip, end_ip)}
{
}

mp::DNSMasqServer::~DNSMasqServer()
{
    dnsmasq_cmd->kill();
    dnsmasq_cmd->waitForFinished();
}

mp::optional<mp::IPAddress> mp::DNSMasqServer::get_ip_for(const std::string& hw_addr)
{
    QProcess cmd;

    cmd.start("bash",
              QStringList() << "-c"
                            << QString("grep \"%1\" %2 | cut -d ' ' -f3")
                                   .arg(QString::fromStdString(hw_addr))
                                   .arg(data_dir.filePath("dnsmasq.leases")));

    cmd.waitForFinished();

    auto ip_addr = cmd.readAll().trimmed();

    if (ip_addr.isEmpty())
        return {};
    else
        return mp::optional<mp::IPAddress>{mp::IPAddress{ip_addr.toStdString()}};
}

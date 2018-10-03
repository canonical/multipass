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

#include <multipass/logging/log.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <fstream>
namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
auto start_dnsmasq_process(const QDir& data_dir, const QString& bridge_name, const mp::IPAddress& bridge_addr,
                           const mp::IPAddress& start_ip, const mp::IPAddress& end_ip)
{
    QString pid;
    auto snap_common = qgetenv("SNAP_COMMON");
    if (!snap_common.isEmpty()) {
        pid = QString("--pid-file=%1/dnsmasq.pid").arg(QString(snap_common));
    }
    auto cmd = std::make_unique<QProcess>();

    QObject::connect(cmd.get(), &QProcess::readyReadStandardError,
                     [&cmd]() { mpl::log(mpl::Level::error, "dnsmasq", cmd->readAllStandardError().data()); });

    cmd->start("dnsmasq",
               QStringList() << "--keep-in-foreground" << pid
                             << "--strict-order"
                             << "--bind-interfaces"
                             << "--except-interface=lo" << QString("--interface=%1").arg(bridge_name)
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
} // namespace

mp::DNSMasqServer::DNSMasqServer(const Path& path, const QString& bridge_name, const IPAddress& bridge_addr,
                                 const IPAddress& start, const IPAddress& end)
    : data_dir{QDir(path)},
      dnsmasq_cmd{start_dnsmasq_process(data_dir, bridge_name, bridge_addr, start, end)},
      bridge_name{bridge_name}
{
}

mp::DNSMasqServer::~DNSMasqServer()
{
    dnsmasq_cmd->kill();
    dnsmasq_cmd->waitForFinished();
}

mp::optional<mp::IPAddress> mp::DNSMasqServer::get_ip_for(const std::string& hw_addr)
{
    // DNSMasq leases entries consist of:
    // <lease expiration> <mac addr> <ipv4> <name> * * *
    const auto path = data_dir.filePath("dnsmasq.leases").toStdString();
    const std::string delimiter{" "};
    const int hw_addr_idx{1};
    const int ipv4_idx{2};
    std::ifstream leases_file{path};
    std::string line;
    while (getline(leases_file, line))
    {
        const auto fields = mp::utils::split(line, delimiter);
        if (fields.size() > 2 && fields[hw_addr_idx] == hw_addr)
            return mp::optional<mp::IPAddress>{fields[ipv4_idx]};
    }
    return mp::nullopt;
}

void mp::DNSMasqServer::release_mac(const std::string& hw_addr)
{
    auto ip = get_ip_for(hw_addr);
    if (!ip)
    {
        mpl::log(mpl::Level::warning, "dnsmasq", fmt::format("attempting to release non-existant addr: {}", hw_addr));
        return;
    }

    QProcess dhcp_release;
    QObject::connect(&dhcp_release, &QProcess::errorOccurred, [&ip, &hw_addr](QProcess::ProcessError error) {
        mpl::log(mpl::Level::warning, "dnsmasq",
                 fmt::format("failed to release ip addr {} with mac {}", ip.value().as_string(), hw_addr));
    });

    auto log_exit_status = [&ip, &hw_addr](int exit_code, QProcess::ExitStatus exit_status) {
        if (exit_code == 0 && exit_status == QProcess::NormalExit)
            return;

        auto msg = fmt::format("failed to release ip addr {} with mac {}, exit_code: {}", ip.value().as_string(),
                               hw_addr, exit_code);
        mpl::log(mpl::Level::warning, "dnsmasq", msg);
    };
    QObject::connect(&dhcp_release, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     log_exit_status);

    dhcp_release.start("dhcp_release", QStringList() << bridge_name << QString::fromStdString(ip.value().as_string())
                                                     << QString::fromStdString(hw_addr));

    dhcp_release.waitForFinished();
}

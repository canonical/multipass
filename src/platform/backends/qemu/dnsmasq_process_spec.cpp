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

#include <multipass/format.h>

namespace mp = multipass;

mp::DNSMasqProcessSpec::DNSMasqProcessSpec(const mp::Path& data_dir, const QString& bridge_name,
                                           const QString& pid_file_path, const std::string& subnet)
    : data_dir(data_dir), bridge_name(bridge_name), pid_file_path{pid_file_path}, subnet{subnet}
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

    return QStringList() << "--strict-order"
                         << "--bind-interfaces" << QString("--pid-file=%1").arg(pid_file_path) << "--domain=multipass"
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

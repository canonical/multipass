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

#ifndef MULTIPASS_DNSMASQ_SERVER_H
#define MULTIPASS_DNSMASQ_SERVER_H

#include <multipass/ip_address.h>
#include <multipass/optional.h>
#include <multipass/path.h>

#include <QDir>
#include <QProcess>

#include <memory>
#include <string>

namespace multipass
{
class DNSMasqServer
{
public:
    DNSMasqServer(const Path& path, const QString& bridge_name, const IPAddress& bridge_addr,
                  const IPAddress& start, const IPAddress& end);
    DNSMasqServer(DNSMasqServer&& other);
    ~DNSMasqServer();

    optional<IPAddress> get_ip_for(const std::string& hw_addr);

private:
    const IPAddress start_ip;
    const IPAddress end_ip;
    const QDir data_dir;
    std::unique_ptr<QProcess> dnsmasq_cmd;
};
}
#endif // MULTIPASS_DNSMASQ_SERVER_H

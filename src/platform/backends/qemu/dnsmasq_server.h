/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#include <QTemporaryFile>

#include <memory>
#include <string>

namespace multipass
{
class Process;

class DNSMasqServer
{
public:
    DNSMasqServer(const Path& data_dir, const QString& bridge_name, const std::string& subnet);
    DNSMasqServer(const DNSMasqServer&) = delete;
    DNSMasqServer& operator=(const DNSMasqServer&) = delete;
    virtual ~DNSMasqServer(); // inherited by mock for testing

    virtual optional<IPAddress> get_ip_for(const std::string& hw_addr);
    void release_mac(const std::string& hw_addr);
    void check_dnsmasq_running();

protected:
    DNSMasqServer() = default; // For testing

private:
    void start_dnsmasq();

    const QString data_dir;
    const QString bridge_name;
    const std::string subnet;
    std::unique_ptr<Process> dnsmasq_cmd;
    QMetaObject::Connection finish_connection;
    QTemporaryFile conf_file;
};
} // namespace multipass
#endif // MULTIPASS_DNSMASQ_SERVER_H

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

#pragma once

#include <multipass/disabled_copy_move.h>
#include <multipass/ip_address.h>
#include <multipass/singleton.h>

#include <QString>
#include <QTemporaryFile>

#include <memory>
#include <optional>
#include <string>

namespace multipass
{
class Process;

class DNSMasqServer : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<DNSMasqServer>;

    DNSMasqServer(const QString& data_dir, const QString& bridge_name, const std::string& subnet);
    virtual ~DNSMasqServer(); // inherited by mock for testing

    virtual std::optional<IPAddress> get_ip_for(const std::string& hw_addr);
    virtual void release_mac(const std::string& hw_addr);
    virtual void check_dnsmasq_running();

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

#define MP_DNSMASQ_SERVER_FACTORY multipass::DNSMasqServerFactory::instance()

class DNSMasqServerFactory : public Singleton<DNSMasqServerFactory>
{
public:
    DNSMasqServerFactory(const Singleton<DNSMasqServerFactory>::PrivatePass& pass) noexcept
        : Singleton<DNSMasqServerFactory>::Singleton{pass} {};

    virtual DNSMasqServer::UPtr make_dnsmasq_server(const QString& network_dir,
                                                    const QString& bridge_name,
                                                    const std::string& subnet) const;
};
} // namespace multipass

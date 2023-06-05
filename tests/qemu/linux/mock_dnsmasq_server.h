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

#ifndef MULTIPASS_MOCK_DNSMASQ_SERVER_H
#define MULTIPASS_MOCK_DNSMASQ_SERVER_H

#include "tests/common.h"
#include "tests/mock_singleton_helpers.h"

#include <src/platform/backends/qemu/linux/dnsmasq_server.h>

namespace multipass
{
namespace test
{
struct MockDNSMasqServer : public DNSMasqServer
{
    using DNSMasqServer::DNSMasqServer; // ctor

    MOCK_METHOD(std::optional<IPAddress>, get_ip_for, (const std::string&), (override));
    MOCK_METHOD(void, release_mac, (const std::string&), (override));
    MOCK_METHOD(void, check_dnsmasq_running, (), (override));
};

struct MockDNSMasqServerFactory : public DNSMasqServerFactory
{
    using DNSMasqServerFactory::DNSMasqServerFactory;

    MOCK_METHOD(DNSMasqServer::UPtr, make_dnsmasq_server, (const Path&, const QString&, const std::string&),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockDNSMasqServerFactory, DNSMasqServerFactory);
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_DNSMASQ_SERVER_H

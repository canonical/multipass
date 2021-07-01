/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#include "tests/mock_platform.h"

#include <src/platform/backends/qemu/linux/dnsmasq_server.h>

namespace multipass
{
namespace test
{
struct MockDNSMasqServer : public DNSMasqServer
{
    using DNSMasqServer::DNSMasqServer; // ctor

    MockDNSMasqServer(const Path& data_dir, const QString& bridge_name, const std::string& subnet){};

    MOCK_METHOD1(get_ip_for, optional<IPAddress>(const std::string&));
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_DNSMASQ_SERVER_H

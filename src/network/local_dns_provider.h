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

#include "dns_server.h"

#include <multipass/dns_provider.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace multipass
{
// Answers queries with a built-in DNS server, listening on the loopback interface.
class LocalDnsProvider final : public DnsProvider
{
public:
    // Points the host's resolver for `domain` at `name_server`, until the returned handle is
    // released.
    using RouteDomain = std::function<std::shared_ptr<void>(const std::string& domain,
                                                            const std::string& name_server)>;

    // Listens on the first free one of `address_count` consecutive IPv4 addresses from `first`, on
    // its port, to steer clear of other servers on the loopback interface.
    LocalDnsProvider(RouteDomain route_domain,
                     const boost::asio::ip::udp::endpoint& first,
                     unsigned address_count = 1);
    ~LocalDnsProvider() override;

    void start(const std::string& domain) override;
    void teardown() noexcept override;
    void set_host(const std::string& host, const std::vector<IPAddress>& addresses) override;
    void remove_host(const std::string& host) override;

private:
    std::unique_ptr<dns::Server> listen(const std::string& zone);
    std::vector<boost::asio::ip::address> lookup(std::string_view host) const;

    const RouteDomain route_domain;
    const boost::asio::ip::udp::endpoint first;
    const unsigned address_count;
    mutable std::mutex mutex;
    std::unordered_map<std::string, std::vector<boost::asio::ip::address>> hosts;
    std::unique_ptr<dns::Server> server;
    std::shared_ptr<void> route;
};
} // namespace multipass

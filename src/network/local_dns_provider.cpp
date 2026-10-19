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

#include "local_dns_provider.h"

#include <multipass/logging/log.h>

#include <boost/asio/error.hpp>
#include <boost/system/system_error.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace asio = boost::asio;

namespace
{
constexpr auto category = "dns";
constexpr std::uint32_t ttl = 30; // in seconds; short, as addresses change
} // namespace

mp::LocalDnsProvider::LocalDnsProvider(RouteDomain route_domain,
                                       const asio::ip::udp::endpoint& first,
                                       unsigned address_count)
    : route_domain{std::move(route_domain)}, first{first}, address_count{address_count}
{
}

mp::LocalDnsProvider::~LocalDnsProvider()
{
    teardown();
}

void mp::LocalDnsProvider::start(const std::string& domain)
{
    auto zone = dns::normalize_name(domain);
    if (zone.empty())
        throw std::invalid_argument{fmt::format("Invalid DNS domain: \"{}\"", domain)};

    teardown();

    // Answer first, then route, so that the host's resolver is never pointed at a server that is
    // not there. If routing fails, the new server goes away with the exception.
    auto new_server = listen(zone);
    route = route_domain(zone, new_server->endpoint().address().to_string());
    server = std::move(new_server);

    mpl::info(category, "Instances resolve as <name>.{}", zone);
}

std::unique_ptr<mp::dns::Server> mp::LocalDnsProvider::listen(const std::string& zone)
{
    auto address = first.address().to_v4().to_uint();
    for (unsigned attempt = 1;; ++attempt, ++address)
    {
        const asio::ip::udp::endpoint endpoint{asio::ip::address_v4{address}, first.port()};
        try
        {
            return std::make_unique<dns::Server>(
                dns::Zone{zone, ttl},
                endpoint,
                [this](std::string_view host) { return lookup(host); });
        }
        catch (const boost::system::system_error& e)
        {
            const auto taken = e.code() == asio::error::address_in_use ||
                               e.code() == asio::error::access_denied;
            if (!taken || attempt >= address_count)
                throw;

            mpl::debug(category,
                       "{}:{} is taken, trying the next address",
                       endpoint.address().to_string(),
                       endpoint.port());
        }
    }
}

void mp::LocalDnsProvider::teardown() noexcept
{
    route.reset(); // un-route first, for the same reason
    server.reset();
}

void mp::LocalDnsProvider::set_host(const std::string& host,
                                    const std::vector<IPAddress>& addresses)
{
    auto name = dns::normalize_name(host);

    std::vector<asio::ip::address_v4> records;
    std::vector<std::string> shown;
    for (const auto& address : addresses)
    {
        records.emplace_back(address.octets);
        shown.push_back(address.as_string());
    }
    mpl::debug(category, "{} resolves to {}", name, fmt::join(shown, ", "));

    const std::lock_guard lock{mutex};
    hosts.insert_or_assign(std::move(name), std::move(records));
}

void mp::LocalDnsProvider::remove_host(const std::string& host)
{
    const auto name = dns::normalize_name(host);

    const std::lock_guard lock{mutex};
    hosts.erase(name);
}

std::vector<asio::ip::address_v4> mp::LocalDnsProvider::lookup(std::string_view host) const
{
    const std::lock_guard lock{mutex};
    const auto it = hosts.find(std::string{host});
    return it == hosts.end() ? std::vector<asio::ip::address_v4>{} : it->second;
}

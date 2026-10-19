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
constexpr std::size_t max_label_size = 63;

std::string parse_host(const std::string& host)
{
    auto name = mp::dns::normalize_name(host);
    if (name.empty() || name.size() > max_label_size || name.find('.') != std::string::npos)
        throw std::invalid_argument{fmt::format("Invalid DNS host name: \"{}\"", host)};
    return name;
}
} // namespace

mp::LocalDnsProvider::LocalDnsProvider(RouteDomain route_domain,
                                       const asio::ip::udp::endpoint& endpoint)
    : route_domain{std::move(route_domain)}, endpoint{endpoint}
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
    auto new_server = std::make_unique<dns::Server>(
        dns::Zone{zone, ttl},
        endpoint,
        [this](std::string_view host) { return lookup(host); });
    route = route_domain(zone, endpoint.address().to_string());
    server = std::move(new_server);

    mpl::info(category, "Instances resolve as <name>.{}", zone);
}

void mp::LocalDnsProvider::teardown() noexcept
{
    route.reset(); // un-route first, for the same reason
    server.reset();
}

void mp::LocalDnsProvider::set_host(const std::string& host,
                                    const std::vector<IPAddress>& addresses)
{
    auto name = parse_host(host);

    std::vector<asio::ip::address> records;
    std::vector<std::string> shown;
    for (const auto& address : addresses)
    {
        records.emplace_back(asio::ip::address_v4{address.octets});
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

std::vector<asio::ip::address> mp::LocalDnsProvider::lookup(std::string_view host) const
{
    const std::lock_guard lock{mutex};
    const auto it = hosts.find(std::string{host});
    return it == hosts.end() ? std::vector<asio::ip::address>{} : it->second;
}

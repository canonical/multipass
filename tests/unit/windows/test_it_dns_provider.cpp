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

#include "tests/unit/common.h"

#include <multipass/ip_address.h>
#include <multipass/platform.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <string>
#include <vector>

namespace mp = multipass;
namespace asio = boost::asio;
using namespace testing;

namespace
{
// Resolves `name` the way applications do (getaddrinfo), through the host's resolver.
std::vector<std::string> resolve(const std::string& name)
{
    asio::io_context io;
    asio::ip::udp::resolver resolver{io};
    boost::system::error_code error;
    const auto results = resolver.resolve(name, "", error);

    std::vector<std::string> addresses;
    if (!error)
        for (const auto& entry : results)
            addresses.push_back(entry.endpoint().address().to_string());
    return addresses;
}
} // namespace

// Needs administrative privileges, to route the domain through the host's resolver.
TEST(PlatformWin_IntegrationTests, dnsProviderResolvesHostsThroughTheHostResolver)
{
    const auto dns = MP_PLATFORM.make_dns_provider();
    dns->set_host("probe", {mp::IPAddress{"10.99.0.42"}});
    dns->start("multipass-it.local");

    EXPECT_THAT(resolve("probe.multipass-it.local"), Contains("10.99.0.42"));
    EXPECT_THAT(resolve("PROBE.Multipass-IT.local"), Contains("10.99.0.42"));
    EXPECT_THAT(resolve("unknown.multipass-it.local"), IsEmpty());
}

// Other local DNS servers may hold the platform's first address: the provider moves on.
TEST(PlatformWin_IntegrationTests, dnsProviderSkipsTakenAddresses)
{
    asio::io_context io;
    // Never answers, so lookups routed here would fail.
    const asio::ip::udp::socket squatter{io, {asio::ip::make_address("127.77.0.1"), 53}};

    const auto dns = MP_PLATFORM.make_dns_provider();
    dns->set_host("probe", {mp::IPAddress{"10.99.0.43"}});
    dns->start("multipass-it-walk.local"); // not cached by the host's resolver, unlike the above

    EXPECT_THAT(resolve("probe.multipass-it-walk.local"), Contains("10.99.0.43"));
}

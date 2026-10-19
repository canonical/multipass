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

#include "common.h"

#include <src/network/dns_server.h>
#include <src/network/local_dns_provider.h>

// After Boost.Asio (via dns_server.h), which brings in the platform's socket headers first.
#include <ares.h>

#include <array>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>

namespace mp = multipass;
namespace asio = boost::asio;
using asio::ip::udp;
using namespace testing;

namespace
{
struct RecordDeleter
{
    void operator()(ares_dns_record_t* record) const noexcept
    {
        ares_dns_record_destroy(record);
    }
};
using Record = std::unique_ptr<ares_dns_record_t, RecordDeleter>;

struct Query
{
    std::string name;
    ares_dns_rec_type_t type = ARES_REC_TYPE_A;
    unsigned short flags = ARES_FLAG_RD;
    ares_dns_class_t qclass = ARES_CLASS_IN;
    ares_dns_opcode_t opcode = ARES_OPCODE_QUERY;
    bool edns = false;
};

std::vector<std::uint8_t> encode(const Query& query)
{
    ares_dns_record_t* raw = nullptr;
    if (ares_dns_record_create(&raw, 0x1234, query.flags, query.opcode, ARES_RCODE_NOERROR) !=
        ARES_SUCCESS)
        throw std::runtime_error{"failed to create query"};
    const Record record{raw};

    ares_dns_record_query_add(record.get(), query.name.c_str(), query.type, query.qclass);
    if (query.edns)
    {
        ares_dns_rr_t* rr = nullptr;
        ares_dns_record_rr_add(&rr,
                               record.get(),
                               ARES_SECTION_ADDITIONAL,
                               "",
                               ARES_REC_TYPE_OPT,
                               ARES_CLASS_IN,
                               0);
        ares_dns_rr_set_u16(rr, ARES_RR_OPT_UDP_SIZE, 4096);
    }

    unsigned char* buffer = nullptr;
    std::size_t size = 0;
    if (ares_dns_write(record.get(), &buffer, &size) != ARES_SUCCESS)
        throw std::runtime_error{"failed to write query"};
    std::vector<std::uint8_t> bytes(buffer, buffer + size);
    ares_free_string(buffer);
    return bytes;
}

// A decoded response, with its records rendered as "<TYPE> <data>" strings.
struct Response
{
    unsigned short id;
    unsigned short flags;
    ares_dns_rcode_t rcode;
    std::vector<std::string> questions;
    std::vector<std::string> answers;
    std::vector<std::string> authority;
    std::vector<unsigned int> ttls;
};

std::string render(const ares_dns_rr_t* rr)
{
    switch (ares_dns_rr_get_type(rr))
    {
    case ARES_REC_TYPE_A:
    {
        asio::ip::address_v4::bytes_type bytes;
        std::memcpy(bytes.data(), ares_dns_rr_get_addr(rr, ARES_RR_A_ADDR), bytes.size());
        return "A " + asio::ip::address_v4{bytes}.to_string();
    }
    case ARES_REC_TYPE_AAAA:
    {
        asio::ip::address_v6::bytes_type bytes;
        std::memcpy(bytes.data(), ares_dns_rr_get_addr6(rr, ARES_RR_AAAA_ADDR), bytes.size());
        return "AAAA " + asio::ip::address_v6{bytes}.to_string();
    }
    case ARES_REC_TYPE_SOA:
        return fmt::format("SOA {} {} {}",
                           ares_dns_rr_get_name(rr),
                           ares_dns_rr_get_str(rr, ARES_RR_SOA_RNAME),
                           ares_dns_rr_get_u32(rr, ARES_RR_SOA_MINIMUM));
    default:
        return fmt::format("TYPE{}", static_cast<int>(ares_dns_rr_get_type(rr)));
    }
}

Response decode(const std::vector<std::uint8_t>& bytes)
{
    ares_dns_record_t* raw = nullptr;
    if (ares_dns_parse(bytes.data(), bytes.size(), 0, &raw) != ARES_SUCCESS)
        throw std::runtime_error{"failed to parse response"};
    const Record record{raw};

    Response response{ares_dns_record_get_id(record.get()),
                      ares_dns_record_get_flags(record.get()),
                      ares_dns_record_get_rcode(record.get()),
                      {},
                      {},
                      {},
                      {}};

    for (std::size_t i = 0; i < ares_dns_record_query_cnt(record.get()); ++i)
    {
        const char* name = nullptr;
        ares_dns_rec_type_t type{};
        ares_dns_class_t qclass{};
        ares_dns_record_query_get(record.get(), i, &name, &type, &qclass);
        response.questions.emplace_back(name);
    }
    for (std::size_t i = 0; i < ares_dns_record_rr_cnt(record.get(), ARES_SECTION_ANSWER); ++i)
    {
        const auto* rr = ares_dns_record_rr_get_const(record.get(), ARES_SECTION_ANSWER, i);
        response.answers.push_back(render(rr));
        response.ttls.push_back(ares_dns_rr_get_ttl(rr));
    }
    for (std::size_t i = 0; i < ares_dns_record_rr_cnt(record.get(), ARES_SECTION_AUTHORITY); ++i)
        response.authority.push_back(
            render(ares_dns_record_rr_get_const(record.get(), ARES_SECTION_AUTHORITY, i)));

    return response;
}

// Sends `query` to `server` over UDP, waiting up to `timeout` for the response.
std::optional<Response> exchange(const udp::endpoint& server,
                                 const Query& query,
                                 std::chrono::milliseconds timeout = std::chrono::seconds{5})
{
    asio::io_context io;
    udp::socket socket{io, udp::endpoint{udp::v4(), 0}};
    socket.send_to(asio::buffer(encode(query)), server);

    std::array<std::uint8_t, 4096> buffer{};
    udp::endpoint sender;
    std::optional<Response> response;
    socket.async_receive_from(
        asio::buffer(buffer),
        sender,
        [&](const boost::system::error_code& error, std::size_t size) {
            if (!error)
                response = decode(std::vector<std::uint8_t>(buffer.begin(), buffer.begin() + size));
        });
    io.run_for(timeout);
    return response;
}

constexpr auto zone_soa = "SOA multipass.local hostmaster.multipass.local 30";

struct DnsMessageTest : public Test
{
    std::optional<std::vector<std::uint8_t>> respond_raw(const std::vector<std::uint8_t>& query)
    {
        return mp::dns::make_response(query, zone, lookup);
    }

    Response respond(const Query& query)
    {
        const auto response = respond_raw(encode(query));
        if (!response)
            throw std::runtime_error{"no response"};
        return decode(*response);
    }

    mp::dns::Zone zone{"multipass.local", 30};
    std::map<std::string, std::vector<asio::ip::address>> hosts{
        {"vm1", {asio::ip::make_address("10.0.0.1")}},
        {"dual", {asio::ip::make_address("10.0.0.2"), asio::ip::make_address("fd00::2")}}};
    mp::dns::HostLookup lookup = [this](std::string_view host) {
        const auto it = hosts.find(std::string{host});
        return it == hosts.end() ? std::vector<asio::ip::address>{} : it->second;
    };
};
} // namespace

TEST_F(DnsMessageTest, answersAQueryForAKnownHost)
{
    const auto response = respond({.name = "vm1.multipass.local"});

    EXPECT_EQ(response.id, 0x1234);
    EXPECT_EQ(response.flags, ARES_FLAG_QR | ARES_FLAG_AA | ARES_FLAG_RD);
    EXPECT_EQ(response.rcode, ARES_RCODE_NOERROR);
    EXPECT_THAT(response.questions, ElementsAre("vm1.multipass.local"));
    EXPECT_THAT(response.answers, ElementsAre("A 10.0.0.1"));
    EXPECT_THAT(response.ttls, ElementsAre(30u));
    EXPECT_THAT(response.authority, IsEmpty());
}

TEST_F(DnsMessageTest, answersAaaaQueryWithIpv6Addresses)
{
    const auto response = respond({.name = "dual.multipass.local", .type = ARES_REC_TYPE_AAAA});

    EXPECT_EQ(response.rcode, ARES_RCODE_NOERROR);
    EXPECT_THAT(response.answers, ElementsAre("AAAA fd00::2"));
}

TEST_F(DnsMessageTest, answersAnyQueryWithAllAddresses)
{
    const auto response = respond({.name = "dual.multipass.local", .type = ARES_REC_TYPE_ANY});

    EXPECT_THAT(response.answers, UnorderedElementsAre("A 10.0.0.2", "AAAA fd00::2"));
}

TEST_F(DnsMessageTest, answersEdnsQueriesWithoutEdns)
{
    const auto response = respond({.name = "vm1.multipass.local", .edns = true});

    EXPECT_THAT(response.answers, ElementsAre("A 10.0.0.1"));
}

TEST_F(DnsMessageTest, repliesNoDataForMissingRecordType)
{
    const auto response = respond({.name = "vm1.multipass.local", .type = ARES_REC_TYPE_AAAA});

    EXPECT_EQ(response.rcode, ARES_RCODE_NOERROR);
    EXPECT_THAT(response.answers, IsEmpty());
    EXPECT_THAT(response.authority, ElementsAre(zone_soa));
}

TEST_F(DnsMessageTest, repliesNxDomainForUnknownHosts)
{
    for (const auto* name : {"unknown.multipass.local", "a.vm1.multipass.local"})
    {
        const auto response = respond({.name = name});

        EXPECT_EQ(response.rcode, ARES_RCODE_NXDOMAIN) << name;
        EXPECT_EQ(response.flags & ARES_FLAG_AA, ARES_FLAG_AA) << name;
        EXPECT_THAT(response.answers, IsEmpty()) << name;
        EXPECT_THAT(response.authority, ElementsAre(zone_soa)) << name;
    }
}

TEST_F(DnsMessageTest, matchesNamesCaseInsensitivelyAndEchoesTheQuestion)
{
    const auto response = respond({.name = "VM1.MultiPass.LOCAL"});

    EXPECT_THAT(response.questions, ElementsAre("VM1.MultiPass.LOCAL"));
    EXPECT_THAT(response.answers, ElementsAre("A 10.0.0.1"));
}

TEST_F(DnsMessageTest, refusesQueriesOutsideTheZone)
{
    for (const auto* name : {"example.com", "vm1.evilmultipass.local", "local"})
    {
        const auto response = respond({.name = name});

        EXPECT_EQ(response.rcode, ARES_RCODE_REFUSED) << name;
        EXPECT_EQ(response.flags & ARES_FLAG_AA, 0) << name;
        EXPECT_THAT(response.answers, IsEmpty()) << name;
    }
}

TEST_F(DnsMessageTest, refusesNonInternetClasses)
{
    const auto response = respond({.name = "vm1.multipass.local", .qclass = ARES_CLASS_CHAOS});

    EXPECT_EQ(response.rcode, ARES_RCODE_REFUSED);
}

TEST_F(DnsMessageTest, answersSoaAtTheZoneApex)
{
    const auto soa = respond({.name = "multipass.local", .type = ARES_REC_TYPE_SOA});
    EXPECT_EQ(soa.rcode, ARES_RCODE_NOERROR);
    EXPECT_THAT(soa.answers, ElementsAre(zone_soa));

    const auto address = respond({.name = "multipass.local"});
    EXPECT_EQ(address.rcode, ARES_RCODE_NOERROR);
    EXPECT_THAT(address.answers, IsEmpty());
    EXPECT_THAT(address.authority, ElementsAre(zone_soa));
}

TEST_F(DnsMessageTest, rejectsUnsupportedOpcodes)
{
    const auto response = respond({.name = "vm1.multipass.local", .opcode = ARES_OPCODE_STATUS});

    EXPECT_EQ(response.rcode, ARES_RCODE_NOTIMP);
    EXPECT_EQ(response.flags & ARES_FLAG_AA, 0);
    EXPECT_THAT(response.questions, ElementsAre("vm1.multipass.local"));
    EXPECT_THAT(response.answers, IsEmpty());
}

TEST_F(DnsMessageTest, dropsMalformedMessages)
{
    // Too short, and a header announcing a question that is missing.
    EXPECT_FALSE(respond_raw({0x12, 0x34, 0x01, 0x00, 0x00}));
    EXPECT_FALSE(respond_raw({0xab, 0xcd, 0x01, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0}));
}

TEST_F(DnsMessageTest, dropsResponses)
{
    EXPECT_FALSE(
        respond_raw(encode({.name = "vm1.multipass.local", .flags = ARES_FLAG_QR | ARES_FLAG_AA})));
}

TEST_F(DnsMessageTest, truncatesResponsesThatDoNotFitInADatagram)
{
    auto& addresses = hosts["many"];
    for (int i = 1; i <= 40; ++i)
        addresses.push_back(asio::ip::make_address(fmt::format("fd00::{:x}", i)));

    const auto response = respond({.name = "many.multipass.local", .type = ARES_REC_TYPE_AAAA});

    EXPECT_EQ(response.flags & ARES_FLAG_TC, ARES_FLAG_TC);
    EXPECT_THAT(response.answers, IsEmpty());
    EXPECT_THAT(response.questions, ElementsAre("many.multipass.local"));
}

TEST(DnsName, normalizesCaseAndDots)
{
    EXPECT_EQ(mp::dns::normalize_name(".Multipass.LOCAL."), "multipass.local");
    EXPECT_EQ(mp::dns::normalize_name("..."), "");
}

TEST(DnsServer, answersQueriesOverUdp)
{
    const mp::dns::Server server{{"multipass.local", 30},
                                 udp::endpoint{asio::ip::make_address("127.0.0.1"), 0},
                                 [](std::string_view host) {
                                     return host == "vm1"
                                              ? std::vector{asio::ip::make_address("10.0.0.1")}
                                              : std::vector<asio::ip::address>{};
                                 }};
    ASSERT_NE(server.endpoint().port(), 0);

    const auto known = exchange(server.endpoint(), {.name = "vm1.multipass.local"});
    ASSERT_TRUE(known);
    EXPECT_THAT(known->answers, ElementsAre("A 10.0.0.1"));

    const auto unknown = exchange(server.endpoint(), {.name = "vm2.multipass.local"});
    ASSERT_TRUE(unknown);
    EXPECT_EQ(unknown->rcode, ARES_RCODE_NXDOMAIN);
}

TEST(DnsServer, throwsWhenTheAddressIsTaken)
{
    const mp::dns::Server first{{"multipass.local", 30},
                                udp::endpoint{asio::ip::make_address("127.0.0.1"), 0},
                                {}};

    EXPECT_THROW(mp::dns::Server({"multipass.local", 30}, first.endpoint(), {}),
                 std::runtime_error);
}

namespace
{
std::uint16_t free_udp_port()
{
    asio::io_context io;
    const udp::socket socket{io, udp::endpoint{asio::ip::make_address("127.0.0.1"), 0}};
    return socket.local_endpoint().port();
}

struct LocalDnsProviderTest : public Test
{
    // Records routing, instead of touching the host's resolver.
    mp::LocalDnsProvider::RouteDomain fake_route = [this](const std::string& domain,
                                                          const std::string& name_server) {
        events.push_back(fmt::format("route {} to {}", domain, name_server));
        return std::shared_ptr<void>{nullptr, [this, domain](void*) {
                                         events.push_back("unroute " + domain);
                                     }};
    };

    std::optional<Response> query(const std::string& name)
    {
        return exchange(endpoint, {.name = name});
    }

    bool answers()
    {
        return exchange(endpoint, {.name = "vm1.multipass.local"}, std::chrono::milliseconds{250})
            .has_value();
    }

    std::vector<std::string> events; // outlives the provider, which unroutes on destruction
    const udp::endpoint endpoint{asio::ip::make_address("127.0.0.1"), free_udp_port()};
    mp::LocalDnsProvider provider{fake_route, endpoint};
};
} // namespace

TEST_F(LocalDnsProviderTest, resolvesHostsThroughTheRoutedServer)
{
    provider.set_host("VM1", {mp::IPAddress{"10.0.0.1"}});
    provider.start("Multipass.Local");

    EXPECT_THAT(events, ElementsAre("route multipass.local to 127.0.0.1"));
    const auto response = query("vm1.multipass.local");
    ASSERT_TRUE(response);
    EXPECT_THAT(response->answers, ElementsAre("A 10.0.0.1"));
}

TEST_F(LocalDnsProviderTest, keepsHostsAcrossRestarts)
{
    provider.set_host("vm1", {mp::IPAddress{"10.0.0.1"}});
    provider.start("first.local");
    provider.start("second.local");

    EXPECT_THAT(events,
                ElementsAre("route first.local to 127.0.0.1",
                            "unroute first.local",
                            "route second.local to 127.0.0.1"));
    const auto response = query("vm1.second.local");
    ASSERT_TRUE(response);
    EXPECT_THAT(response->answers, ElementsAre("A 10.0.0.1"));
}

TEST_F(LocalDnsProviderTest, stopsResolvingRemovedHosts)
{
    provider.set_host("vm1", {mp::IPAddress{"10.0.0.1"}});
    provider.start("multipass.local");
    provider.remove_host("VM1");

    const auto response = query("vm1.multipass.local");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->rcode, ARES_RCODE_NXDOMAIN);
}

TEST_F(LocalDnsProviderTest, teardownUnroutesAndStopsAnswering)
{
    provider.start("multipass.local");
    provider.teardown();
    provider.teardown(); // idempotent

    EXPECT_THAT(events,
                ElementsAre("route multipass.local to 127.0.0.1", "unroute multipass.local"));
    EXPECT_FALSE(answers());
}

TEST_F(LocalDnsProviderTest, failingToRouteLeavesNothingBehind)
{
    fake_route = [](const std::string&, const std::string&) -> std::shared_ptr<void> {
        throw std::runtime_error{"access denied"};
    };
    mp::LocalDnsProvider failing{fake_route, endpoint};

    EXPECT_THROW(failing.start("multipass.local"), std::runtime_error);
    EXPECT_FALSE(answers());
    EXPECT_NO_THROW(provider.start("multipass.local")); // the port is free again
    EXPECT_TRUE(answers());
}

TEST_F(LocalDnsProviderTest, listensOnTheNextAddressWhenOneIsTaken)
{
    const udp::endpoint next{asio::ip::make_address("127.0.0.2"), endpoint.port()};
    asio::io_context io;
    {
        udp::socket probe{io, udp::v4()};
        boost::system::error_code error;
        probe.bind(next, error);
        if (error)
            GTEST_SKIP() << "The loopback interface lacks 127.0.0.2: " << error.message();
    }

    const udp::socket squatter{io, endpoint};
    mp::LocalDnsProvider walking{fake_route, endpoint, 2};
    walking.set_host("vm1", {mp::IPAddress{"10.0.0.1"}});
    walking.start("multipass.local");

    EXPECT_THAT(events, ElementsAre("route multipass.local to 127.0.0.2"));
    const auto response = exchange(next, {.name = "vm1.multipass.local"});
    ASSERT_TRUE(response);
    EXPECT_THAT(response->answers, ElementsAre("A 10.0.0.1"));
}

TEST_F(LocalDnsProviderTest, failsWhenEveryAddressIsTaken)
{
    asio::io_context io;
    const udp::socket squatter{io, endpoint};

    EXPECT_THROW(provider.start("multipass.local"), boost::system::system_error);
    EXPECT_THAT(events, IsEmpty());
}

TEST_F(LocalDnsProviderTest, rejectsInvalidDomainsAndHosts)
{
    EXPECT_THROW(provider.start(".."), std::invalid_argument);
    EXPECT_THAT(events, IsEmpty());

    const std::vector address{mp::IPAddress{"10.0.0.1"}};
    EXPECT_THROW(provider.set_host("", address), std::invalid_argument);
    EXPECT_THROW(provider.set_host("a.b", address), std::invalid_argument);
    EXPECT_THROW(provider.set_host(std::string(64, 'a'), address), std::invalid_argument);
}

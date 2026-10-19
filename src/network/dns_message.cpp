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

#include "dns_message.h"

// After Boost.Asio (via dns_message.h), which brings in the platform's socket headers first.
#include <ares.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace mp = multipass;
namespace asio = boost::asio;

namespace
{
constexpr std::size_t max_udp_payload = 512; // RFC 1035, without EDNS

struct RecordDeleter
{
    void operator()(ares_dns_record_t* record) const noexcept
    {
        ares_dns_record_destroy(record);
    }
};
using Record = std::unique_ptr<ares_dns_record_t, RecordDeleter>;

void check(ares_status_t status)
{
    if (status != ARES_SUCCESS)
        throw std::runtime_error{ares_strerror(status)};
}

Record make_record(unsigned short id,
                   unsigned short flags,
                   ares_dns_opcode_t opcode,
                   ares_dns_rcode_t rcode)
{
    ares_dns_record_t* record = nullptr;
    check(ares_dns_record_create(&record, id, flags, opcode, rcode));
    return Record{record};
}

std::vector<std::uint8_t> serialize(const ares_dns_record_t* record)
{
    unsigned char* buffer = nullptr;
    std::size_t size = 0;
    check(ares_dns_write(record, &buffer, &size));

    const std::unique_ptr<unsigned char, decltype(&ares_free_string)> owner{buffer,
                                                                            &ares_free_string};
    return std::vector<std::uint8_t>(buffer, buffer + size);
}

void add_soa(ares_dns_record_t* record, ares_dns_section_t section, const mp::dns::Zone& zone)
{
    ares_dns_rr_t* rr = nullptr;
    check(ares_dns_record_rr_add(&rr,
                                 record,
                                 section,
                                 zone.name.c_str(),
                                 ARES_REC_TYPE_SOA,
                                 ARES_CLASS_IN,
                                 zone.ttl));

    const auto mailbox = "hostmaster." + zone.name;
    check(ares_dns_rr_set_str(rr, ARES_RR_SOA_MNAME, zone.name.c_str()));
    check(ares_dns_rr_set_str(rr, ARES_RR_SOA_RNAME, mailbox.c_str()));
    check(ares_dns_rr_set_u32(rr, ARES_RR_SOA_SERIAL, 1));
    check(ares_dns_rr_set_u32(rr, ARES_RR_SOA_REFRESH, 3600));
    check(ares_dns_rr_set_u32(rr, ARES_RR_SOA_RETRY, 600));
    check(ares_dns_rr_set_u32(rr, ARES_RR_SOA_EXPIRE, 86400));
    check(ares_dns_rr_set_u32(rr, ARES_RR_SOA_MINIMUM, zone.ttl)); // negative caching TTL
}

void add_address(ares_dns_record_t* record,
                 const char* name,
                 const asio::ip::address& address,
                 std::uint32_t ttl)
{
    ares_dns_rr_t* rr = nullptr;
    if (address.is_v4())
    {
        check(ares_dns_record_rr_add(&rr,
                                     record,
                                     ARES_SECTION_ANSWER,
                                     name,
                                     ARES_REC_TYPE_A,
                                     ARES_CLASS_IN,
                                     ttl));
        const auto bytes = address.to_v4().to_bytes();
        in_addr addr{};
        std::memcpy(&addr, bytes.data(), bytes.size());
        check(ares_dns_rr_set_addr(rr, ARES_RR_A_ADDR, &addr));
    }
    else
    {
        check(ares_dns_record_rr_add(&rr,
                                     record,
                                     ARES_SECTION_ANSWER,
                                     name,
                                     ARES_REC_TYPE_AAAA,
                                     ARES_CLASS_IN,
                                     ttl));
        const auto bytes = address.to_v6().to_bytes();
        ares_in6_addr addr{};
        std::memcpy(&addr, bytes.data(), bytes.size());
        check(ares_dns_rr_set_addr6(rr, ARES_RR_AAAA_ADDR, &addr));
    }
}

bool matches(ares_dns_rec_type_t type, const asio::ip::address& address)
{
    return type == ARES_REC_TYPE_ANY || (type == ARES_REC_TYPE_A && address.is_v4()) ||
           (type == ARES_REC_TYPE_AAAA && address.is_v6());
}

struct Answer
{
    ares_dns_rcode_t rcode = ARES_RCODE_NOERROR;
    bool authoritative = true;
    bool soa_answer = false;    // the zone's SOA, as the answer
    bool soa_authority = false; // the zone's SOA, for negative caching
    std::vector<asio::ip::address> addresses{};
};

Answer answer(std::string_view name,
              ares_dns_rec_type_t type,
              ares_dns_class_t qclass,
              const mp::dns::Zone& zone,
              const mp::dns::HostLookup& lookup)
{
    if (qclass != ARES_CLASS_IN && qclass != ARES_CLASS_ANY)
        return {.rcode = ARES_RCODE_REFUSED, .authoritative = false};

    if (name == zone.name)
    {
        if (type == ARES_REC_TYPE_SOA || type == ARES_REC_TYPE_ANY)
            return {.soa_answer = true};
        return {.soa_authority = true};
    }

    // Not recursive: the host's resolver is only meant to send us queries for our own zone.
    if (!name.ends_with("." + zone.name))
        return {.rcode = ARES_RCODE_REFUSED, .authoritative = false};

    // Hosts are single labels, directly under the zone.
    const auto host = name.substr(0, name.size() - zone.name.size() - 1);
    auto addresses = host.find('.') == std::string_view::npos ? lookup(host)
                                                              : std::vector<asio::ip::address>{};
    if (addresses.empty())
        return {.rcode = ARES_RCODE_NXDOMAIN, .soa_authority = true};

    std::erase_if(addresses, [type](const auto& address) { return !matches(type, address); });
    if (addresses.empty())
        return {.soa_authority = true}; // NODATA: the host exists, without records of this type

    return {.addresses = std::move(addresses)};
}
} // namespace

std::string mp::dns::normalize_name(std::string_view name)
{
    while (name.starts_with('.'))
        name.remove_prefix(1);
    while (name.ends_with('.'))
        name.remove_suffix(1);

    std::string normalized{name};
    std::ranges::transform(normalized, normalized.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return normalized;
}

std::optional<std::vector<std::uint8_t>> mp::dns::make_response(std::span<const std::uint8_t> query,
                                                                const Zone& zone,
                                                                const HostLookup& lookup)
try
{
    // Malformed messages are dropped, rather than answered with a format error.
    ares_dns_record_t* parsed = nullptr;
    if (ares_dns_parse(query.data(), query.size(), 0, &parsed) != ARES_SUCCESS)
        return std::nullopt;
    const Record request{parsed};

    const auto id = ares_dns_record_get_id(request.get());
    const auto opcode = ares_dns_record_get_opcode(request.get());
    const auto query_flags = ares_dns_record_get_flags(request.get());
    if (query_flags & ARES_FLAG_QR || ares_dns_record_query_cnt(request.get()) != 1)
        return std::nullopt; // a response, or a query with no single question

    const char* qname = nullptr;
    ares_dns_rec_type_t qtype{};
    ares_dns_class_t qclass{};
    check(ares_dns_record_query_get(request.get(), 0, &qname, &qtype, &qclass));

    const auto result = opcode == ARES_OPCODE_QUERY
                          ? answer(normalize_name(qname), qtype, qclass, zone, lookup)
                          : Answer{.rcode = ARES_RCODE_NOTIMP, .authoritative = false};

    // The question is echoed as received, preserving its case for the client to match.
    const auto compose = [&](bool truncated) {
        const auto flags = static_cast<unsigned short>(ARES_FLAG_QR | (query_flags & ARES_FLAG_RD) |
                                                       (result.authoritative ? ARES_FLAG_AA : 0) |
                                                       (truncated ? ARES_FLAG_TC : 0));
        const auto response = make_record(id, flags, opcode, result.rcode);
        check(ares_dns_record_query_add(response.get(), qname, qtype, qclass));
        if (!truncated)
        {
            for (const auto& address : result.addresses)
                add_address(response.get(), qname, address, zone.ttl);
            if (result.soa_answer)
                add_soa(response.get(), ARES_SECTION_ANSWER, zone);
            if (result.soa_authority)
                add_soa(response.get(), ARES_SECTION_AUTHORITY, zone);
        }
        return serialize(response.get());
    };

    // Only with an unusual number of addresses; the client is told to retry over TCP, which
    // this server does not serve.
    if (auto response = compose(false); response.size() <= max_udp_payload)
        return response;
    return compose(true);
}
catch (const std::exception&)
{
    return std::nullopt;
}

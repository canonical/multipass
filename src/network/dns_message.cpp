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

std::vector<std::uint8_t> serialize(const ares_dns_record_t* record)
{
    unsigned char* buffer = nullptr;
    std::size_t size = 0;
    check(ares_dns_write(record, &buffer, &size));

    const std::unique_ptr<unsigned char, decltype(&ares_free_string)> owner{buffer,
                                                                            &ares_free_string};
    return std::vector<std::uint8_t>(buffer, buffer + size);
}

void add_address(ares_dns_record_t* record,
                 const char* name,
                 const asio::ip::address_v4& address,
                 std::uint32_t ttl)
{
    ares_dns_rr_t* rr = nullptr;
    check(ares_dns_record_rr_add(&rr,
                                 record,
                                 ARES_SECTION_ANSWER,
                                 name,
                                 ARES_REC_TYPE_A,
                                 ARES_CLASS_IN,
                                 ttl));

    const auto bytes = address.to_bytes();
    in_addr addr{};
    std::memcpy(&addr, bytes.data(), bytes.size());
    check(ares_dns_rr_set_addr(rr, ARES_RR_A_ADDR, &addr));
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
    ares_dns_record_t* parsed = nullptr;
    if (ares_dns_parse(query.data(), query.size(), 0, &parsed) != ARES_SUCCESS)
        return std::nullopt;
    const Record request{parsed};

    const auto query_flags = ares_dns_record_get_flags(request.get());
    if (query_flags & ARES_FLAG_QR ||
        ares_dns_record_get_opcode(request.get()) != ARES_OPCODE_QUERY ||
        ares_dns_record_query_cnt(request.get()) != 1)
        return std::nullopt;

    const char* qname = nullptr;
    ares_dns_rec_type_t qtype{};
    ares_dns_class_t qclass{};
    check(ares_dns_record_query_get(request.get(), 0, &qname, &qtype, &qclass));

    // The host's resolver only sends us names in the zone.
    const auto name = normalize_name(qname);
    const auto suffix = "." + zone.name;
    const auto apex = name == zone.name;
    if (qclass != ARES_CLASS_IN || !(apex || name.ends_with(suffix)))
        return std::nullopt;

    // Hosts are single labels, directly under the zone, which itself exists without records.
    std::vector<asio::ip::address_v4> addresses;
    if (const auto host = std::string_view{name}.substr(0, name.size() - suffix.size());
        !apex && host.find('.') == std::string_view::npos)
        addresses = lookup(host);

    ares_dns_record_t* created = nullptr;
    check(ares_dns_record_create(
        &created,
        ares_dns_record_get_id(request.get()),
        static_cast<unsigned short>(ARES_FLAG_QR | ARES_FLAG_AA | (query_flags & ARES_FLAG_RD)),
        ARES_OPCODE_QUERY,
        apex || !addresses.empty() ? ARES_RCODE_NOERROR : ARES_RCODE_NXDOMAIN));
    const Record response{created};

    // The question is echoed as received, preserving its case for the client to match.
    check(ares_dns_record_query_add(response.get(), qname, qtype, qclass));
    if (qtype == ARES_REC_TYPE_A)
        for (const auto& address : addresses)
            add_address(response.get(), qname, address, zone.ttl);

    return serialize(response.get());
}
catch (const std::exception&)
{
    return std::nullopt;
}

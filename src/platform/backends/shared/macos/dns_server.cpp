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

#include "dns_server.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <netinet/in.h>
#include <sys/socket.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "dns";

constexpr std::uint16_t qclass_in = 1;
constexpr std::size_t max_udp_msg = 512; // As per RFC 1035
constexpr std::size_t dns_header_len = 12;
constexpr std::uint16_t qr_mask = 0x8000; // Header bit mask

// Decoded DNS query message as described in RFC 1035
// Only the required header and the question are parsed
struct Query
{
    std::uint16_t id;
    std::uint16_t flags;
    std::string qname;
    std::uint16_t qtype;
    std::uint16_t qclass;
    std::size_t question_end;
};

std::uint16_t read_u16(std::span<const unsigned char> p)
{
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

std::optional<Query> parse_msg(std::spane<const unsigned char> msg)
{
    const auto len = msg.size();
    if (len < dns_header_len) // Invalid length for DNS header
        return std::nullopt;

    Query q{};
    q.id = read_u16(msg);        // first byte
    q.flags = read_u16(msg.subspan(2)); // next 2 bytes
    const auto qdcount = read_u16(msg.subspan(4));

    // msg type is query (0) and verify only one question
    // https://www.ietf.org/archive/id/draft-bellis-dnsop-qdcount-is-one-00.html#name-updates-to-rfc-1035
    if ((q.flags & qr_mask != 0) || qdcount != 1)
        return std::nullopt;

    // Parse the labels in the message
    std::size_t pos = dns_header_len;
    std::string name;
    while (true)
    {
        if (pos >= len) // Invalid label length
            return std::nullopt;

        const unsigned char label_len = msg[pos++];
        if (label_len == 0) // 0 means we found the root label
            break;
        if ((label_len & 0xc0) !=
            0) // reject if compression pointer or reserved bits; signified by the top 2 bits
            return std::nullopt;
        if (pos + label_len > len) // Data does not equal label length
            return std::nullopt;

        if (!name.empty())
            name.push_back('.');
        std::ranges::transform(msg.subspan(pos, label_len),
                               std::back_inserter(name),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        pos += label_len;
    }

    if (pos + 4 > len) // Invalid number of bytes left after root label
        return std::nullopt;

    q.qname = std::move(name);
    q.qtype = read_u16(msg.subspan(pos));
    q.qclass = read_u16(msg.subspan(pos + 2));
    q.question_end = pos + 4;

    return q;
}

std::vector<unsigned char> build_response()
{
    return {};
}
}

mp::MacDNSServer::MacDNSServer(const std::string& domain, std::uint16_t port, Resolver resolver)
    : domain{domain}, resolver{std::move(resolver)}
{
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
        throw std::runtime_error{
            fmt::format("failed to create DNS server socket: {}", std::strerror(errno))};

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr) > 0))
    {
        const auto err = std::strerror(errno);
        close(socket_fd);
        socket_fd = -1;
        throw std::runtime_error{
            fmt::format("failed to bind socket on 127.0.0.1:{}: {}", port, err)};
    }

    listener = std::jthread{[this](std::stop_token stop_token) { run(std::move(stop_token)); }};

    mpl::info(category, "DNS resolver listening on 127.0.0.1:{} for *.{}", port, domain);
}

mp::MacDNSServer::~MacDNSServer()
{
    listener.request_stop();

    if (socket_fd >= 0)
        close(socket_fd);
}

void mp::MacDNSServer::run(std::stop_token stop_token) noexcept
{
    std::array<unsigned char, max_udp_msg> buffer{};

    // UDP socket communication loop
    while (!stop_token.stop_requested())
    {
        sockaddr_in client{};
        socklen_t client_len = sizeof(client);
        const auto msg_len = recvfrom(socket_fd,
                                      buffer.data(),
                                      buffer.size(),
                                      0,
                                      reinterpret_cast<sockaddr*>(&client),
                                      &client_len);

        if (msg_len <= 0)
        {
            if (msg_len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                mpl::debug(category, "recvfrom failed: {}", std::strerror(errno));
            continue;
        }

        const auto datagram = std::span{buffer}.first(static_cast<std::size_t>(received));
        const auto query = parse_msg(buffer.data(), static_cast<std::size_t>(msg_len));
        if (!query)
        {
            mpl::debug(category, "Failed to parse message");
            continue;
        }

        std::optional<mp::IPAddress> ip;
        if (query->qclass == qclass_in && query->qtype == qtype_a)
        {
            // Strip the trailing ".<domain>" to resolve the instance name.
            const std::string suffix = "." + domain;
            if (query->qname.size() > suffix.size() && query->qname.ends_with(suffix))
            {
                const auto instance_name =
                    query->qname.substr(0, query->qname.size() - suffix.size());
                try
                {
                    ip = resolver(instance_name);
                }
                catch (const std::exception& e)
                    mpl::warn(category,
                              "Unexpected error occured resolving an IP address for \"{}\": {}",
                              instance_name,
                              e.what());
            }
        }

        const auto resp = build_response();

        sendto(socket_fd,
               resp.data(),
               resp.size(),
               0,
               reinterpret_cast<sockaddr*>(&client),
               client_len);
    }
}

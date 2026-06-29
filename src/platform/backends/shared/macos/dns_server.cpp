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

#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "dns";

// RFC 1035 constants
constexpr std::uint16_t qtype_a = 1;
constexpr std::uint16_t qclass_in = 1;
constexpr std::size_t max_udp_msg = 512;
constexpr std::size_t dns_header_len = 12;
constexpr std::uint32_t answer_ttl = 0; // Do not let client cache IPs

// Header flags/masks
constexpr std::uint16_t qr_mask = 0x8000;            // Header bit mask
constexpr std::uint16_t authoritative_flag = 0x0400; // AA = 1
constexpr std::uint16_t rcode_nxdomain = 0x0003;     // RCODE = 3

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

void append_u16(std::vector<unsigned char>& out, std::uint16_t val)
{
    out.push_back(static_cast<unsigned char>(val >> 8));
    out.push_back(static_cast<unsigned char>(val & 0xff));
}

void append_u32(std::vector<unsigned char>& out, std::uint32_t val)
{
    out.push_back(static_cast<unsigned char>((val >> 24) & 0xff));
    out.push_back(static_cast<unsigned char>((val >> 16) & 0xff));
    out.push_back(static_cast<unsigned char>((val >> 8) & 0xff));
    out.push_back(static_cast<unsigned char>(val & 0xff));
}

// Parses a raw DNS message into a Query object
//
// We put extra care into verifying the message form because this comes from a UDP socket that is
// connectionless and spoofable
std::optional<Query> parse_msg(std::span<const unsigned char> msg)
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
    if ((q.flags & qr_mask) != 0 || qdcount != 1)
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

// Build a DNS response message for the given parsed query
//
// The response always echoes back the original question section verbatim so the client can match
// the reply to its question
std::vector<unsigned char> build_response(std::span<const unsigned char> query,
                                          const Query& q,
                                          const std::optional<mp::IPAddress>& ip)
{
    std::vector<unsigned char> retval;
    retval.reserve(q.question_end + 16); // Answer record is 16 bytes long

    const bool resolved = ip.has_value();
    std::uint16_t flags = qr_mask | authoritative_flag | (q.flags & 0x0100); // preserve RD
    if (!resolved)
        flags |= rcode_nxdomain;

    append_u16(retval, q.id);
    append_u16(retval, flags);
    append_u16(retval, 1);                // QDCOUNT
    append_u16(retval, resolved ? 1 : 0); // ANCOUNT
    append_u16(retval, 0);                // NSCOUNT
    append_u16(retval, 0);                // ARCOUNT

    // Echo the question section verbatim
    retval.insert(retval.end(), query.begin() + dns_header_len, query.begin() + q.question_end);

    if (resolved)
    {
        append_u16(retval, 0xc00c);     // NAME: internal pointer to the question name at offset 12
        append_u16(retval, qtype_a);    // TYPE: A
        append_u16(retval, qclass_in);  // CLASS: IN
        append_u32(retval, answer_ttl); // TTL
        append_u16(retval, 4);          // RDLENGTH

        // insert IP address octets
        retval.insert(retval.end(), ip->octets.begin(), ip->octets.end());
    }

    return retval;
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

    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        const auto err = std::strerror(errno);
        close(socket_fd);
        socket_fd = -1;
        throw std::runtime_error{
            fmt::format("failed to bind socket on 127.0.0.1:{}: {}", port, err)};
    }

    // macOS does not interrupt a blocked recvfrom()/poll() via shutdown() on an unconnected
    // datagram socket, so `stop_pipe_write` is used to signal the `run` loop to terminate from the
    // destructor.
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0)
    {
        const auto err = std::strerror(errno);
        close(socket_fd);
        socket_fd = -1;
        throw std::runtime_error(fmt::format("failed to create DNS stop pipe: {}", err));
    }

    stop_pipe_read = pipe_fds[0];
    stop_pipe_write = pipe_fds[1];

    // Stop child processes from inheriting the pipe fds
    if (fcntl(stop_pipe_read, F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(stop_pipe_write, F_SETFD, FD_CLOEXEC) < 0)
        mpl::warn(category, "failed to set FD_CLOEXEC on DNS self pipe: {}", std::strerror(errno));

    listener = std::jthread{[this](std::stop_token stop_token) { run(std::move(stop_token)); }};

    mpl::info(category, "DNS resolver listening on 127.0.0.1:{} for *.{}", port, domain);
}

mp::MacDNSServer::~MacDNSServer()
{
    listener.request_stop();

    if (stop_pipe_write >= 0)
    {
        const char wake = 'x';
        write(stop_pipe_write, &wake, 1); // Unblock poll()
    }

    if (listener.joinable())
        listener.join();

    if (stop_pipe_write >= 0)
        close(stop_pipe_write);
    if (stop_pipe_read >= 0)
        close(stop_pipe_read);
    if (socket_fd >= 0)
        close(socket_fd);
}

void mp::MacDNSServer::run(std::stop_token stop_token) noexcept
{
    std::array<unsigned char, max_udp_msg> buffer{};

    // UDP socket communication loop
    while (!stop_token.stop_requested())
    {
        pollfd fds[2]{{.fd = socket_fd, .events = POLLIN},
                      {.fd = stop_pipe_read, .events = POLLIN}};

        // Wait for DNS socket or self-pipe
        if (const auto ready = poll(fds, 2, -1); ready < 0)
        {
            if (errno != EINTR)
                mpl::debug(category, "poll failed: {}", std::strerror(errno));
            continue;
        }

        if (fds[1].revents & POLLIN) // Shutdown requested via self-pipe
            break;
        if (!(fds[0].revents & POLLIN))
            continue;

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

        const auto datagram = std::span{buffer}.first(static_cast<std::size_t>(msg_len));
        const auto query = parse_msg(datagram);
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
                {
                    mpl::warn(category,
                              "Unexpected error occured resolving an IP address for \"{}\": {}",
                              instance_name,
                              e.what());
                }
            }
        }

        const auto resp = build_response(datagram, *query, ip);

        sendto(socket_fd,
               resp.data(),
               resp.size(),
               0,
               reinterpret_cast<sockaddr*>(&client),
               client_len);
    }
}

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

constexpr std::size_t max_udp_msg = 512; // As per RFC 1035

void parse_msg(const unsigned char* msg, std::size_t len)
{
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

        parse_msg(buffer.data(), static_cast<std::size_t>(msg_len));

        const auto resp = build_response();

        sendto(socket_fd,
               resp.data(),
               resp.size(),
               0,
               reinterpret_cast<sockaddr*>(&client),
               client_len);
    }
}

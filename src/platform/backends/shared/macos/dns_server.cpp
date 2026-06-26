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
}

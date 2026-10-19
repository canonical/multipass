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

#include <multipass/logging/log.h>

#include <boost/asio/buffer.hpp>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace asio = boost::asio;
using asio::ip::udp;

namespace
{
constexpr auto category = "dns-server";
} // namespace

mp::dns::Server::Server(Zone zone, const udp::endpoint& endpoint, HostLookup lookup)
    : zone{std::move(zone)},
      lookup{std::move(lookup)},
      socket{io, endpoint},
      bound_port{socket.local_endpoint().port()}
{
    receive();
    thread = std::thread{[this] { io.run(); }};

    mpl::debug(category,
               "Serving DNS zone {} on {}:{}",
               this->zone.name,
               endpoint.address().to_string(),
               bound_port);
}

mp::dns::Server::~Server()
{
    io.stop();
    thread.join();
}

std::uint16_t mp::dns::Server::port() const
{
    return bound_port;
}

void mp::dns::Server::receive()
{
    socket.async_receive_from(
        asio::buffer(buffer),
        sender,
        [this](const boost::system::error_code& error, std::size_t size) {
            if (error == asio::error::operation_aborted)
                return;

            // Errors are per datagram (e.g. a client that went away), so keep serving.
            if (error)
                mpl::debug(category, "Failed to receive a DNS query: {}", error.message());
            else if (const auto response = make_response({buffer.data(), size}, zone, lookup))
            {
                boost::system::error_code send_error;
                socket.send_to(asio::buffer(*response), sender, 0, send_error);
                if (send_error)
                    mpl::debug(category, "Failed to send a DNS response: {}", send_error.message());
            }

            receive();
        });
}

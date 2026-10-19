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

#pragma once

#include "dns_message.h"

#include <multipass/disabled_copy_move.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <array>
#include <cstdint>
#include <thread>

namespace multipass::dns
{
// Answers DNS queries for a zone over UDP, from its own thread, until destroyed.
class Server : private DisabledCopyMove
{
public:
    // Listens on `endpoint` (port 0 picks a free one), looking hosts up with `lookup`, which is
    // called from the server's thread. Throws boost::system::system_error (a std::runtime_error)
    // if the endpoint cannot be bound.
    Server(Zone zone, const boost::asio::ip::udp::endpoint& endpoint, HostLookup lookup);
    ~Server();

    [[nodiscard]] const boost::asio::ip::udp::endpoint& endpoint() const;

private:
    void receive();

    const Zone zone;
    const HostLookup lookup;
    boost::asio::io_context io;
    boost::asio::ip::udp::socket socket;
    // Cached, as the socket is used from the server's thread.
    const boost::asio::ip::udp::endpoint bound_endpoint;
    std::array<std::uint8_t, 4096> buffer{};
    boost::asio::ip::udp::endpoint sender;
    std::thread thread;
};
} // namespace multipass::dns

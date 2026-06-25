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

#include <multipass/disabled_copy_move.h>
#include <multipass/base_dns_server.h>

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace multipass
{
// A minimal authoritative DNS responder for a single custom domain (e.g. "multipass"),
// answering A-record queries for "<name>.<domain>" on a loopback UDP port. Intended to be
// paired with an `/etc/resolver/<domain>` file so the macOS system resolver forwards
// matching queries here.
class MacDNSServer : public BaseDNSServer, private DisabledCopyMove
{
public:
    MacDNSServer(const std::string& domain, std::uint16_t port, Resolver resolver);

private:
    void run(std::stop_token stop_token) noexcept;

    const std::string domain;
    const std::uint16_t port;
    const Resolver resolver;
    int socket_fd{-1};
    std::jthread listener;
};
} // namespace multipass

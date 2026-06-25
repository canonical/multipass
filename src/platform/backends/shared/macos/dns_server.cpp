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

namespace mp = multipass;

mp::MacDNSServer::MacDNSServer(const std::string& domain, std::uint16_t port, Resolver resolver)
    : domain{domain}, resolver{std::move(resolver)}
{
}

mp::MacDNSServer::~MacDNSServer()
{
}

void mp::MacDNSServer::run(std::stop_token stop_token) noexcept
{
}

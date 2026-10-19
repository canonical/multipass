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

#include <boost/asio/ip/address.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace multipass::dns
{
// The zone a server is authoritative for.
struct Zone
{
    std::string name; // normalized, e.g. "multipass.local"
    std::uint32_t ttl;
};

// Returns the addresses of `host` (a single, lower-case label), or none if it is unknown.
using HostLookup = std::function<std::vector<boost::asio::ip::address>(std::string_view host)>;

// Lower-cases `name` and strips its leading and trailing dots.
std::string normalize_name(std::string_view name);

// Builds the response to `query` from a server authoritative for `zone`, looking hosts up with
// `lookup`. Returns nothing when the message must be dropped: when it is malformed or not a
// query, or when the response cannot be built.
std::optional<std::vector<std::uint8_t>> make_response(std::span<const std::uint8_t> query,
                                                       const Zone& zone,
                                                       const HostLookup& lookup);
} // namespace multipass::dns

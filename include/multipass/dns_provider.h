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
#include <multipass/ip_address.h>

#include <memory>
#include <string>
#include <vector>

namespace multipass
{
/**
 * Makes instances resolvable by name from the host, as "<host>.<domain>".
 *
 * Hides how: what answers the queries (a built-in DNS server, or one the platform already runs)
 * and how the host's resolver is pointed at it (e.g. NRPT, /etc/resolver, systemd-resolved).
 * Destroying a started provider tears it down.
 */
class DnsProvider : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<DnsProvider>;

    virtual ~DnsProvider() = default;

    /**
     * Starts resolving "<host>.<domain>" on the host, tearing down a previous start() first.
     *
     * @throws std::invalid_argument if the domain is invalid.
     * @throws std::runtime_error if it cannot start, leaving nothing behind.
     */
    virtual void start(const std::string& domain) = 0;

    /**
     * Stops resolving on the host. Idempotent.
     */
    virtual void teardown() noexcept = 0;

    /**
     * Makes `host` resolve to `addresses`, replacing previous ones. Hosts are kept across
     * start() and teardown().
     *
     * @throws std::invalid_argument if `host` is not a valid DNS label.
     */
    virtual void set_host(const std::string& host, const std::vector<IPAddress>& addresses) = 0;

    /**
     * Stops `host` from resolving. Does nothing if it is unknown.
     */
    virtual void remove_host(const std::string& host) = 0;
};
} // namespace multipass

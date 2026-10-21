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

#include <multipass/constants.h>

#include <cstdint>
#include <optional>
#include <string>

namespace multipass
{
/* Sequence of candidate ports to try, starting at a preferred port and moving
 * up. Remembers where the search stopped, so a candidate that turns out to be
 * unusable after being handed out does not come up again: just ask for the
 * next one. Every candidate considered is logged under the "port" category. */
class PortCandidateGenerator
{
public:
    PortCandidateGenerator(std::uint16_t preferred_port,
                           int max_attempts = default_max_port_attempts);

    /* The next candidate that is free, or nothing if the search ran out of
     * attempts. Advances past the returned port, so subsequent calls resume
     * the search where the previous one stopped. */
    [[nodiscard]] std::optional<std::uint16_t> next_free();

private:
    std::uint16_t preferred;
    std::uint32_t next_candidate;
    int attempts_left;
};

/* Access to the daemon port setting in the registry: the single source of
 * truth that the daemon and all clients agree on. */
class ServerPortConfig
{
public:
    /* Machine-wide location (in the registry) of the TCP port that the
     * Multipass daemon listens on. The daemon runs as a Windows service, so
     * this is a per-machine setting that all clients (CLI, GUI) read to find
     * the daemon. Text is UTF-8, converted to UTF-16 where the registry API
     * is called. The defaults are the constants
     * mp::windows_daemon_port_registry_key and
     * mp::windows_daemon_port_registry_value. */
    struct RegistryLocation
    {
        enum class Root
        {
            local_machine,
            current_user,
        };

        Root root = Root::local_machine;
        std::string key_path = windows_daemon_port_registry_key;
        std::string value_name = windows_daemon_port_registry_value;
    };

    explicit ServerPortConfig(RegistryLocation location);
    ServerPortConfig(); /* the default (production) location */

    /* The configured port, for clients looking for the daemon. Reads the
     * registry every time on purpose: that value is the state shared by all
     * Multipass processes, which an in-process copy would only hide. */
    [[nodiscard]] std::optional<std::uint16_t> configured() const;

    /* The port to open the server on: the configured one if it is usable, or
     * the next free candidate otherwise - picked, logged (every attempt) and
     * persisted, so that clients will find the server opened on it. Throws
     * std::runtime_error with remediation guidance if another daemon already
     * owns the configured port, if no free port can be found, or if a newly
     * picked port cannot be persisted. */
    [[nodiscard]] std::uint16_t usable_port(
        std::uint16_t preferred_port = default_grpc_server_tcp_port,
        int max_attempts = default_max_port_attempts) const;

    /* The address for the daemon to listen on, combining the user's
     * preferences in order: the --address option, the
     * MULTIPASS_SERVER_ADDRESS environment variable, the configured registry
     * value, a free candidate port. A pinned address (option or environment,
     * whichever is given; empty means not given) is verified and returned
     * as-is; otherwise the usable registry port (or the next free candidate,
     * persisted) is returned as a localhost address. */
    [[nodiscard]] std::string resolve_address(const std::string& cli_address,
                                              const std::string& env_address,
                                              int max_attempts = default_max_port_attempts) const;

private:
    RegistryLocation location;
};
} // namespace multipass

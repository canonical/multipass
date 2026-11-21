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
/* Check that the daemon can listen on the given address: throw a clear error
 * naming the process in the way - and how to resolve it - if its TCP port is
 * not available. Does nothing for unix sockets. */
void throw_if_address_in_use(const std::string& server_address);

/* Read-only access to the machine-wide daemon port setting in the registry:
 * the single source of truth that the daemon and all clients agree on. */
class ServerPortConfig
{
public:
    /* Location (in the registry) of the TCP port that the Multipass daemon
     * listens on. The daemon runs as a Windows service, so this is a
     * per-machine setting that all clients (CLI, GUI) read to find the
     * daemon. Text is UTF-8, converted to UTF-16 where the registry API is
     * called. The defaults are the constants mp::windows_daemon_port_registry_key
     * and mp::windows_daemon_port_registry_value. */
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

    ServerPortConfig(); /* the default (production) location */
    explicit ServerPortConfig(RegistryLocation location);

    /* The configured port, for clients looking for the daemon. Reads the
     * registry every time on purpose: that value is the state shared by all
     * Multipass processes, which an in-process copy would only hide. */
    [[nodiscard]] std::optional<std::uint16_t> configured() const;

private:
    RegistryLocation location;
};
} // namespace multipass

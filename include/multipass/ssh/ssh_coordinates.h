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

#include <cstdint>
#include <string>
#include <variant>

namespace multipass
{
/**
 * @brief Identifies a Hyper-V socket host.
 *
 * When selected in SSHCoordinates::vsock_host, it directs the SSH session to use the
 * Hyper-V socket transport (instead of plain TCP), addressing the guest by its VM id.
 */
struct HVSOCK
{
    /// VM identifier used to address the Hyper-V socket.
    std::string vmid;
};

/**
 * @brief Identifies an AF_VSOCK host.
 *
 * When selected in SSHCoordinates::vsock_host, it directs the SSH session to use the
 * AF_VSOCK transport (instead of plain TCP), addressing the peer by its context id.
 */
struct VSOCK
{
    /// Context identifier (CID) of the VSOCK peer.
    uint32_t cid;
};

/**
 * @brief Identifies a Unix domain socket host.
 *
 * When selected in SSHCoordinates::vsock_host, it directs the SSH session to use a
 * Unix domain socket transport (instead of plain TCP), addressing it by filesystem path.
 */
struct USOCK
{
    /// Filesystem path/address of the Unix domain socket.
    std::string socket_address;
};

/**
 * @brief Variant selecting the socket transport for an SSH session.
 *
 * Holds exactly one of:
 * - std::monostate: no alternative transport; use plain TCP (SSHCoordinates::tcp_host/port).
 * - HVSOCK: Hyper-V socket, addressed by a vmid.
 * - VSOCK:  AF_VSOCK, addressed by a CID.
 * - USOCK:  Unix domain socket, addressed by a filesystem path.
 */
using VSOCKHost = std::variant<std::monostate, HVSOCK, VSOCK, USOCK>;

/**
 * @brief Coordinates required to open an SSH session to a Multipass VM.
 *
 * This is the canonical, serialisable description of how to reach a VM over SSH and the
 * main interface used to create SSH sessions. It is produced by VirtualMachine subclasses
 * (see BaseVirtualMachine::ssh_coordinates()) and consumed by SSHFactory::make_session()
 * to construct a concrete SSHSession (e.g. PlainSSHSession), which uses these fields to set
 * up the underlying libssh connection. It can also be marshalled to/from the
 * SSHCoordinatesInfo protobuf for gRPC transport (see multipass::utils in grpc_utils.h).
 */
struct SSHCoordinates
{
    /// SSH username to authenticate as on the guest (e.g. "ubuntu").
    std::string username;

    /// Base64-encoded private key used for public-key authentication; decoded and imported
    /// into libssh by SSHFactory::make_key().
    std::string private_key_as_base64;

    /// TCP port the guest's SSH server listens on (typically 22). Used with tcp_host.
    uint32_t port;

    /// Hostname or IP address of the SSH server; used when vsock_host is std::monostate
    /// (plain TCP transport).
    std::string tcp_host;

    /// Selects the optional vsock transport protocol: one of HVSOCK/VSOCK/USOCK to use an
    /// alternative socket transport. See VSOCKHost.
    std::variant<std::monostate, HVSOCK, VSOCK, USOCK> vsock_host;
};
} // namespace multipass

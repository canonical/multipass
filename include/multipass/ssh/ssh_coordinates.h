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

#ifndef __cplusplus
extern "C"
#else
namespace multipass
#endif
{
enum VsockHostTag : uint32_t
{
    VSOCK_NONE = 0,
    VSOCK_HVSOCK = 1,
    VSOCK_VSOCK = 2,
    VSOCK_USOCK = 3
};
}

namespace multipass
{
/// Hyper-V socket transport, addressing the guest by VM id.
struct HVSOCK
{
    std::string vmid;
};

/// AF_VSOCK transport, addressing the peer by context id (CID).
struct VSOCK
{
    uint32_t cid;
};

/// Unix domain socket transport, addressing it by filesystem path.
struct USOCK
{
    std::string socket_address;
};

/// Selects the SSH transport: std::monostate for plain TCP, or one of the vsock families.
using VSOCKHost = std::variant<std::monostate, HVSOCK, VSOCK, USOCK>;

/// Necessary data to reach a VM over ssh
struct SSHCoordinates
{
    std::string username;
    std::string private_key_as_base64;
    uint32_t port;
    std::string tcp_host;
    VSOCKHost vsock_host; ///< Optional vsock transport
};
} // namespace multipass

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
#include <type_traits>
#include <variant>

namespace multipass
{
extern "C"
{
enum VsockTag : uint32_t
{
    VSOCKTAG_NONE = 0,
    VSOCKTAG_HVSOCK = 1,
    VSOCKTAG_VSOCK = 2,
    VSOCKTAG_USOCK = 3,
    VSOCKTAG_SIZE,
};
} // extern "C"

/// Hyper-V socket transport, addressing the guest by VM id.
struct HVSOCKData
{
    std::string vmid;
};

/// AF_VSOCK transport, addressing the peer by context id (CID).
struct VSOCKData
{
    uint32_t cid;
};

/// Unix domain socket transport, addressing it by filesystem path.
struct USOCKData
{
    std::string socket_address;
};

/// Selects the SSH transport: std::monostate for plain TCP, or one of the vsock families.
using VSOCKHost = std::variant<std::monostate, HVSOCKData, VSOCKData, USOCKData>;

// Enforce compile-time equality between C Enum values and std::variant positions
static_assert(std::is_same_v<std::variant_alternative_t<VSOCKTAG_NONE, VSOCKHost>, std::monostate>,
              "VSOCKTAG_NONE mismatch!");
static_assert(std::is_same_v<std::variant_alternative_t<VSOCKTAG_HVSOCK, VSOCKHost>, HVSOCKData>,
              "VSOCKTAG_HVSOCK mismatch!");
static_assert(std::is_same_v<std::variant_alternative_t<VSOCKTAG_VSOCK, VSOCKHost>, VSOCKData>,
              "VSOCKTAG_VSOCK mismatch!");
static_assert(std::is_same_v<std::variant_alternative_t<VSOCKTAG_USOCK, VSOCKHost>, USOCKData>,
              "VSOCKTAG_USOCK mismatch!");
static_assert(std::variant_size_v<VSOCKHost> == VSOCKTAG_SIZE,
              "VSOCKHost count does not match VSOCKTAG_SIZE!");

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

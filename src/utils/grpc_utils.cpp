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

#include <multipass/utils/grpc_utils.h>

#include <cassert>
#include <type_traits>

namespace multipass
{
namespace utils
{
SSHCoordinates proto_to_coordinates(const SSHCoordinatesInfo& proto)
{
    VSOCKHost vsock_host;
    switch (proto.vsock_host_case())
    {
    case SSHCoordinatesInfo::kHvsockVmid:
        vsock_host = HVSOCKData{proto.hvsock_vmid()};
        break;
    case SSHCoordinatesInfo::kVsockCid:
        vsock_host = VSOCKData{proto.vsock_cid()};
        break;
    case SSHCoordinatesInfo::kUsockAddr:
        vsock_host = USOCKData{proto.usock_addr()};
        break;
    case SSHCoordinatesInfo::VSOCK_HOST_NOT_SET:
        vsock_host = std::monostate{};
        break;
    default:
        assert(false && "we should not reach here");
    }
    return {proto.username(), proto.priv_key_base64(), proto.port(), proto.tcp_host(), vsock_host};
}

SSHCoordinatesInfo coordinates_to_proto(const SSHCoordinates& ssh_coordinates)
{
    SSHCoordinatesInfo coordinates_info{};
    coordinates_info.set_username(ssh_coordinates.username);
    coordinates_info.set_priv_key_base64(ssh_coordinates.private_key_as_base64);
    coordinates_info.set_port(ssh_coordinates.port);
    coordinates_info.set_tcp_host(ssh_coordinates.tcp_host);
    std::visit(
        [&coordinates_info](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, HVSOCKData>)
                coordinates_info.set_hvsock_vmid(arg.vmid);
            else if constexpr (std::is_same_v<T, VSOCKData>)
                coordinates_info.set_vsock_cid(arg.cid);
            else if constexpr (std::is_same_v<T, USOCKData>)
                coordinates_info.set_usock_addr(arg.socket_address);
            else if constexpr (std::is_same_v<T, std::monostate>)
                coordinates_info.clear_vsock_host();
        },
        ssh_coordinates.vsock_host);
    return coordinates_info;
}
} // namespace utils
} // namespace multipass

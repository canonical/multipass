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

#include <multipass/reply_concepts.h>
#include <multipass/ssh/ssh_coordinates.h>
#include <multipass/user_messages.h>

#include <multipass/rpc/multipass.grpc.pb.h>

namespace multipass
{
namespace utils
{
template <LogMsgReply Reply, typename Request>
void send_messages(grpc::ServerReaderWriterInterface<Reply, Request>* server,
                   const UserMessages& message_bag)
{
    auto reply = Reply{};
    for (const auto& message : message_bag)
    {
        reply.set_reply_message(message);
        server->Write(reply);
    }
}

/**
 * @brief Convert an SSHCoordinatesInfo protobuf into an SSHCoordinates struct.
 *
 * Maps the scalar fields (username, priv_key_base64, port, tcp_host) directly, and converts
 * the protobuf `vsock_host` oneof into the SSHCoordinates::vsock_host variant:
 * - kHvsockVmid        -> HVSOCK{vmid}
 * - kVsockCid          -> VSOCK{cid}
 * - kUsockAddr         -> USOCK{socket_address}
 * - VSOCK_HOST_NOT_SET -> std::monostate (no vsock transport)
 *
 * The switch is expected to exhaustively cover every oneof case; the default branch asserts,
 * guarding against states that should be unreachable (e.g. a protobuf schema mismatch).
 *
 * @param proto The SSHCoordinatesInfo message received over gRPC.
 * @return The equivalent in-memory SSHCoordinates.
 * @see coordinates_to_proto for the inverse conversion.
 */
SSHCoordinates proto_to_coordinates(const SSHCoordinatesInfo& proto);

/**
 * @brief Convert an SSHCoordinates struct into an SSHCoordinatesInfo protobuf.
 *
 * Copies the scalar fields (username, priv_key_base64, port, tcp_host) directly, then uses
 * std::visit over the vsock_host variant to populate the matching protobuf oneof field:
 * - HVSOCK         -> set_hvsock_vmid()
 * - VSOCK          -> set_vsock_cid()
 * - USOCK          -> set_usock_addr()
 * - std::monostate -> clear_vsock_host() (no alternative transport)
 *
 * This is the inverse of proto_to_coordinates(), enabling lossless round-tripping over gRPC.
 *
 * @param ssh_coordinates The in-memory coordinates to serialise.
 * @return The equivalent SSHCoordinatesInfo message ready for gRPC transmission.
 * @see proto_to_coordinates for the inverse conversion.
 */
SSHCoordinatesInfo coordinates_to_proto(const SSHCoordinates& ssh_coordinates);
} // namespace utils
} // namespace multipass

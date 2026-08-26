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
 * Copies the scalar fields and maps the `vsock_host` oneof onto the vsock_host variant
 * (VSOCK_HOST_NOT_SET -> std::monostate). Inverse of coordinates_to_proto().
 */
SSHCoordinates proto_to_coordinates(const SSHCoordinatesInfo& proto);

/**
 * @brief Convert an SSHCoordinates struct into an SSHCoordinatesInfo protobuf.
 *
 * Copies the scalar fields and maps the vsock_host variant onto the matching `vsock_host`
 * oneof (std::monostate clears it). Inverse of proto_to_coordinates().
 */
SSHCoordinatesInfo coordinates_to_proto(const SSHCoordinates& ssh_coordinates);
} // namespace utils
} // namespace multipass

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
#include <multipass/user_messages.h>

#include <multipass/rpc/multipass.grpc.pb.h>

#include <fmt/format.h>

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

grpc::Status concatenate_status(const grpc::Status& s1, const grpc::Status& s2)
{
    if (s1.ok())
        return s2;
    if (s2.ok())
        return s1;

    auto code = s1.error_code();
    auto msg = fmt::format("{}\n{}", s1.error_message(), s2.error_message());

    return grpc::Status{code, msg};
}
} // namespace utils
} // namespace multipass

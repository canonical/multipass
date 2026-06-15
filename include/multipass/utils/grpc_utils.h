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

#include <multipass/annotated_value.h>
#include <multipass/reply_concepts.h>

#include <grpc++/grpc++.h>

namespace multipass
{
namespace utils
{

template <typename Reply, typename Request>
    requires LogMsgReply<Reply>
void send_messages(grpc::ServerReaderWriterInterface<Reply, Request>* server,
                   MessageBag&& message_bag)
{
    auto reply = Reply{};
    for (const auto& message : message_bag)
    {
        reply.set_reply_message(message);
        server->Write(reply);
    }
}
} // namespace utils
} // namespace multipass

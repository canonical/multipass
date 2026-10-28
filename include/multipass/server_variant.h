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

#include <variant>

namespace grpc
{
template <typename W, typename R>
class ServerReaderWriterInterface;
} // namespace grpc

namespace multipass
{
class StartReply;
class StartRequest;
class LaunchReply;
class LaunchRequest;
class MountReply;
class MountRequest;
class RestartReply;
class RestartRequest;

using ServerVariant =
    std::variant<grpc::ServerReaderWriterInterface<StartReply, StartRequest>*,
                 grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>*,
                 grpc::ServerReaderWriterInterface<MountReply, MountRequest>*,
                 grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>*>;
} // namespace multipass

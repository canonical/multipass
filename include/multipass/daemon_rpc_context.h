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

#include <multipass/logging/client_logger.h>

#include <grpc/status.h>

#include <future>
#include <optional>

namespace multipass
{

struct DaemonRpcContext
{
    virtual void set_value(grpc::Status status) = 0;
    virtual ~DaemonRpcContext() = default;
};

template <typename T, typename U>
struct DaemonRpcContextImpl : DaemonRpcContext, private multipass::DisabledCopyMove
{
    DaemonRpcContextImpl(std::promise<grpc::Status>& promise,
                         grpc::ServerReaderWriterInterface<T, U>* server,
                         logging::Level level,
                         logging::MultiplexingLogger& mpx)
        : promise(promise),
          // We have to construct the logger here since we can't move or copy it.
          logger{std::make_optional<logging::ClientLogger<T, U>>(level, mpx, server)}
    {
    }

    void set_value(grpc::Status status) override
    {
        // Free any resources that depend on server here.
        logger.reset();
        promise.set_value(std::move(status));
    }

private:
    std::promise<grpc::Status>& promise;
    std::optional<logging::ClientLogger<T, U>> logger;
};

} // namespace multipass

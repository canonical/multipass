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

#ifndef MULTIPASS_CLIENT_LOGGER_H
#define MULTIPASS_CLIENT_LOGGER_H

#include <multipass/logging/logger.h>
#include <multipass/logging/multiplexing_logger.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/utils.h>

#include <fmt/format.h>

namespace multipass
{
namespace logging
{
template <typename T, typename U>
class ClientLogger : public Logger
{
public:
    ClientLogger(Level level, MultiplexingLogger& mpx, grpc::ServerReaderWriterInterface<T, U>* server)
        : logging_level{level}, server{server}, mpx_logger{mpx}
    {
        mpx_logger.add_logger(this);
    }

    ~ClientLogger()
    {
        mpx_logger.remove_logger(this);
    }

    void log(Level level, CString category, CString message) const override
    {
        if (level <= logging_level && server != nullptr)
        {
            T reply;
            reply.set_log_line(fmt::format("[{}] [{}] [{}] {}\n", timestamp(), as_string(level).c_str(),
                                           category.c_str(), message.c_str()));
            server->Write(reply);
        }
    }

private:
    Level logging_level;
    grpc::ServerReaderWriterInterface<T, U>* server;
    MultiplexingLogger& mpx_logger;
};
} // namespace logging
} // namespace multipass

#endif // MULTIPASS_CLIENT_LOGGER_H

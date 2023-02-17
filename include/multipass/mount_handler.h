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

#ifndef MULTIPASS_MOUNT_HANDLER_H
#define MULTIPASS_MOUNT_HANDLER_H

#include <multipass/file_ops.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/vm_mount.h>

#include <chrono>
#include <variant>

namespace multipass
{
class VirtualMachine;

using ServerVariant = std::variant<grpc::ServerReaderWriterInterface<StartReply, StartRequest>*,
                                   grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>*,
                                   grpc::ServerReaderWriterInterface<MountReply, MountRequest>*,
                                   grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>*>;

class MountHandler : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<MountHandler>;

    virtual ~MountHandler() = default;

    void start(ServerVariant server, std::chrono::milliseconds timeout = std::chrono::minutes(5))
    {
        std::lock_guard active_lock{active_mutex};
        if (!is_active())
            start_impl(server, timeout);
        active = true;
    }

    void stop(bool force = false)
    {
        std::lock_guard active_lock{active_mutex};
        if (is_active())
            stop_impl(force);
        active = false;
    }

    virtual bool is_active()
    {
        return active;
    }

protected:
    MountHandler() = default;
    MountHandler(VirtualMachine* vm, const SSHKeyProvider* ssh_key_provider, const std::string& target,
                 const std::string& source)
        : vm{vm}, ssh_key_provider{ssh_key_provider}, target{target}, source{source}, active{false}
    {
        std::error_code err;
        auto source_status = MP_FILEOPS.status(source, err);
        if (source_status.type() == fs::file_type::not_found)
            throw std::runtime_error(fmt::format("Mount source path \"{}\" does not exist.", source));
        if (err)
            throw std::runtime_error(
                fmt::format("Mount source path \"{}\" is not accessible: {}.", source, err.message()));
        if (source_status.type() != fs::file_type::directory)
            throw std::runtime_error(fmt::format("Mount source path \"{}\" is not a directory.", source));
        if (source_status.permissions() != fs::perms::unknown &&
            (source_status.permissions() & fs::perms::owner_read) == fs::perms::none)
            throw std::runtime_error(fmt::format("Mount source path \"{}\" is not readable.", source));
    };

    virtual void start_impl(ServerVariant server, std::chrono::milliseconds timeout) = 0;
    virtual void stop_impl(bool force) = 0;

    template <typename Reply, typename Request>
    static Reply make_reply_from_server(grpc::ServerReaderWriterInterface<Reply, Request>*)
    {
        return Reply{};
    }

    template <typename Reply, typename Request>
    static Request make_request_from_server(grpc::ServerReaderWriterInterface<Reply, Request>*)
    {
        return Request{};
    }

    VirtualMachine* vm;
    const SSHKeyProvider* ssh_key_provider;
    const std::string target;
    const std::string source;

    bool active;
    std::mutex active_mutex;
};
} // namespace multipass
#endif // MULTIPASS_MOUNT_HANDLER_H

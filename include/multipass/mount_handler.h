/*
 * Copyright (C) 2022 Canonical, Ltd.
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

struct MountHandler : private DisabledCopyMove
{
    using UPtr = std::unique_ptr<MountHandler>;

    virtual ~MountHandler() = default;

    virtual void start(ServerVariant server, std::chrono::milliseconds timeout = std::chrono::minutes(5)) = 0;
    virtual void stop() = 0;

protected:
    MountHandler() = default;
    MountHandler(VirtualMachine* vm, const SSHKeyProvider* ssh_key_provider, std::string target, const VMMount& mount)
        : vm{vm}, ssh_key_provider{ssh_key_provider}, target{std::move(target)}
    {
        std::error_code err;
        auto source_status = MP_FILEOPS.status(mount.source_path, err);
        if (source_status.type() == fs::file_type::not_found)
            throw std::runtime_error(fmt::format("Mount source path \"{}\" does not exist.", mount.source_path));
        if (err)
            throw std::runtime_error(
                fmt::format("Mount source path \"{}\" is not accessible: {}.", mount.source_path, err.message()));
        if (source_status.type() != fs::file_type::directory)
            throw std::runtime_error(fmt::format("Mount source path \"{}\" is not a directory.", mount.source_path));
        if (source_status.permissions() != fs::perms::unknown &&
            (source_status.permissions() & fs::perms::owner_read) == fs::perms::none)
            throw std::runtime_error(fmt::format("Mount source path \"{}\" is not readable.", mount.source_path));
    };
    

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

    VirtualMachine* vm{};
    const SSHKeyProvider* ssh_key_provider{};
    const std::string target;

};
} // namespace multipass
#endif // MULTIPASS_MOUNT_HANDLER_H

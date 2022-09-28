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

#include <multipass/disabled_copy_move.h>
#include <multipass/id_mappings.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/ssh/ssh_key_provider.h>

#include <chrono>
#include <memory>
#include <string>
#include <variant>

namespace multipass
{
class VirtualMachine;
struct VMMount;

using ServerVariant = std::variant<grpc::ServerReaderWriterInterface<StartReply, StartRequest>*,
                                   grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>*,
                                   grpc::ServerReaderWriterInterface<MountReply, MountRequest>*,
                                   grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>*>;

class MountHandler : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<MountHandler>;

    virtual ~MountHandler() = default;

    // Used to set up anything host side related
    virtual void init_mount(VirtualMachine* vm, const std::string& target_path, const VMMount& vm_mount) = 0;
    virtual void start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                             const std::chrono::milliseconds& timeout = std::chrono::minutes(5)) = 0;
    virtual void stop_mount(const std::string& instance, const std::string& path) = 0;
    virtual void stop_all_mounts_for_instance(const std::string& instance) = 0;
    virtual bool has_instance_already_mounted(const std::string& instance, const std::string& path) const = 0;

protected:
    MountHandler(const SSHKeyProvider& ssh_key_provider) : ssh_key_provider(&ssh_key_provider){};

    template <typename Reply, typename Request>
    Reply make_reply_from_server(grpc::ServerReaderWriterInterface<Reply, Request>&)
    {
        return Reply{};
    }

    const SSHKeyProvider* ssh_key_provider;
};

} // namespace multipass
#endif // MULTIPASS_MOUNT_HANDLER_H

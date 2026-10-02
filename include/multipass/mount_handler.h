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

#include <multipass/file_ops.h>
#include <multipass/guest_mount_record.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/vm_mount.h>

#include <chrono>
#include <string_view>
#include <variant>

namespace multipass
{
class VirtualMachine;

using ServerVariant =
    std::variant<grpc::ServerReaderWriterInterface<StartReply, StartRequest>*,
                 grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>*,
                 grpc::ServerReaderWriterInterface<MountReply, MountRequest>*,
                 grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>*,
                 grpc::ServerReaderWriterInterface<SnapshotReply, SnapshotRequest>*>;

class NativeMountNeedsStoppedVMException : public std::runtime_error
{
public:
    NativeMountNeedsStoppedVMException(const std::string& vm_name);
};

class MountHandler : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<MountHandler>;

    virtual ~MountHandler() = default;

    void activate(ServerVariant server,
                  std::chrono::milliseconds timeout = std::chrono::minutes(5));
    void deactivate(bool force = false);

    const VMMount& get_mount_spec() const noexcept;
    virtual bool is_active();

protected:
    MountHandler(VirtualMachine* vm,
                 const SSHKeyProvider* ssh_key_provider,
                 VMMount mount_spec,
                 const std::string& target);

    virtual void activate_impl(ServerVariant server, std::chrono::milliseconds timeout) = 0;
    virtual void deactivate_impl(bool force) = 0;

    template <typename Reply, typename Request>
    static Reply make_reply_from_server(grpc::ServerReaderWriterInterface<Reply, Request>*);

    template <typename Reply, typename Request>
    static Request make_request_from_server(grpc::ServerReaderWriterInterface<Reply, Request>*);

    VirtualMachine* vm = nullptr;
    const SSHKeyProvider* ssh_key_provider = nullptr;
    const VMMount mount_spec = {};
    const std::string target;
    const std::string& source = mount_spec.get_source_path();
    bool active = false;
    std::mutex active_mutex;

private:
    // Records are best-effort: an unreachable instance only warrants a warning
    void update_record(void (GuestMountRecord::*update)(VirtualMachine&) const,
                       std::string_view action) const;
    void warn_record_failure(std::string_view action, const std::exception& e) const;

    const GuestMountRecord record{source, target};
};
} // namespace multipass

inline multipass::NativeMountNeedsStoppedVMException::NativeMountNeedsStoppedVMException(
    const std::string& vm_name)
    : std::runtime_error(
          fmt::format("Please stop the instance {} before attempting native mounts.", vm_name))
{
}

inline const multipass::VMMount& multipass::MountHandler::get_mount_spec() const noexcept
{
    return mount_spec;
}

inline bool multipass::MountHandler::is_active()
{
    return active;
}

template <typename Reply, typename Request>
Reply multipass::MountHandler::make_reply_from_server(
    grpc::ServerReaderWriterInterface<Reply, Request>*)
{
    return Reply{};
}

template <typename Reply, typename Request>
Request multipass::MountHandler::make_request_from_server(
    grpc::ServerReaderWriterInterface<Reply, Request>*)
{
    return Request{};
}

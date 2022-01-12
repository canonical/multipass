/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_DAEMON_H
#define MULTIPASS_MOCK_DAEMON_H

#include "common.h"

#include <src/daemon/daemon.h>
#include <src/daemon/daemon_rpc.h>

namespace multipass
{
namespace test
{
struct MockDaemon : public Daemon
{
    using Daemon::Daemon;

    MOCK_METHOD3(create,
                 void(const CreateRequest*, grpc::ServerWriterInterface<CreateReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(launch,
                 void(const LaunchRequest*, grpc::ServerWriterInterface<LaunchReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(purge,
                 void(const PurgeRequest*, grpc::ServerWriterInterface<PurgeReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(find, void(const FindRequest* request, grpc::ServerWriterInterface<FindReply>*,
                            std::promise<grpc::Status>*));
    MOCK_METHOD3(info, void(const InfoRequest*, grpc::ServerWriterInterface<InfoReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(list, void(const ListRequest*, grpc::ServerWriterInterface<ListReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(mount, void(const MountRequest* request, grpc::ServerWriterInterface<MountReply>*,
                             std::promise<grpc::Status>*));
    MOCK_METHOD3(recover,
                 void(const RecoverRequest*, grpc::ServerWriterInterface<RecoverReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(ssh_info,
                 void(const SSHInfoRequest*, grpc::ServerWriterInterface<SSHInfoReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(start,
                 void(const StartRequest*, grpc::ServerWriterInterface<StartReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(stop, void(const StopRequest*, grpc::ServerWriterInterface<StopReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(suspend,
                 void(const SuspendRequest*, grpc::ServerWriterInterface<SuspendReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(restart,
                 void(const RestartRequest*, grpc::ServerWriterInterface<RestartReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(delet,
                 void(const DeleteRequest*, grpc::ServerWriterInterface<DeleteReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(umount,
                 void(const UmountRequest*, grpc::ServerWriterInterface<UmountReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(version,
                 void(const VersionRequest*, grpc::ServerWriterInterface<VersionReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(get, void(const GetRequest*, grpc::ServerWriterInterface<GetReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(networks, void(const NetworksRequest*, grpc::ServerWriterInterface<NetworksReply>*,
                                std::promise<grpc::Status>*));
    MOCK_METHOD3(authenticate, void(const AuthenticateRequest*, grpc::ServerWriterInterface<AuthenticateReply>*,
                                    std::promise<grpc::Status>*));

    template <typename Request, typename Reply>
    void set_promise_value(const Request*, grpc::ServerWriterInterface<Reply>*,
                           std::promise<grpc::Status>* status_promise)
    {
        status_promise->set_value(grpc::Status::OK);
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_TESTS_MOCK_DAEMON_H

/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

    MOCK_METHOD3(create, void(const CreateRequest*, grpc::ServerReaderWriterInterface<CreateReply, CreateRequest>*,
                              std::promise<grpc::Status>*));
    MOCK_METHOD3(launch, void(const LaunchRequest*, grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>*,
                              std::promise<grpc::Status>*));
    MOCK_METHOD3(purge, void(const PurgeRequest*, grpc::ServerReaderWriterInterface<PurgeReply, PurgeRequest>*,
                             std::promise<grpc::Status>*));
    MOCK_METHOD3(find, void(const FindRequest* request, grpc::ServerReaderWriterInterface<FindReply, FindRequest>*,
                            std::promise<grpc::Status>*));
    MOCK_METHOD3(info, void(const InfoRequest*, grpc::ServerReaderWriterInterface<InfoReply, InfoRequest>*,
                            std::promise<grpc::Status>*));
    MOCK_METHOD3(list, void(const ListRequest*, grpc::ServerReaderWriterInterface<ListReply, ListRequest>*,
                            std::promise<grpc::Status>*));
    MOCK_METHOD3(mount, void(const MountRequest* request, grpc::ServerReaderWriterInterface<MountReply, MountRequest>*,
                             std::promise<grpc::Status>*));
    MOCK_METHOD3(recover, void(const RecoverRequest*, grpc::ServerReaderWriterInterface<RecoverReply, RecoverRequest>*,
                               std::promise<grpc::Status>*));
    MOCK_METHOD3(ssh_info, void(const SSHInfoRequest*, grpc::ServerReaderWriterInterface<SSHInfoReply, SSHInfoRequest>*,
                                std::promise<grpc::Status>*));
    MOCK_METHOD3(start, void(const StartRequest*, grpc::ServerReaderWriterInterface<StartReply, StartRequest>*,
                             std::promise<grpc::Status>*));
    MOCK_METHOD3(stop, void(const StopRequest*, grpc::ServerReaderWriterInterface<StopReply, StopRequest>*,
                            std::promise<grpc::Status>*));
    MOCK_METHOD3(suspend, void(const SuspendRequest*, grpc::ServerReaderWriterInterface<SuspendReply, SuspendRequest>*,
                               std::promise<grpc::Status>*));
    MOCK_METHOD3(restart, void(const RestartRequest*, grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>*,
                               std::promise<grpc::Status>*));
    MOCK_METHOD3(delet, void(const DeleteRequest*, grpc::ServerReaderWriterInterface<DeleteReply, DeleteRequest>*,
                             std::promise<grpc::Status>*));
    MOCK_METHOD3(umount, void(const UmountRequest*, grpc::ServerReaderWriterInterface<UmountReply, UmountRequest>*,
                              std::promise<grpc::Status>*));
    MOCK_METHOD3(version, void(const VersionRequest*, grpc::ServerReaderWriterInterface<VersionReply, VersionRequest>*,
                               std::promise<grpc::Status>*));
    MOCK_METHOD3(keys, void(const KeysRequest*, grpc::ServerReaderWriterInterface<KeysReply, KeysRequest>*,
                            std::promise<grpc::Status>*));
    MOCK_METHOD3(get, void(const GetRequest*, grpc::ServerReaderWriterInterface<GetReply, GetRequest>*,
                           std::promise<grpc::Status>*));
    MOCK_METHOD3(set, void(const SetRequest*, grpc::ServerReaderWriterInterface<SetReply, SetRequest>*,
                           std::promise<grpc::Status>*));
    MOCK_METHOD3(networks,
                 void(const NetworksRequest*, grpc::ServerReaderWriterInterface<NetworksReply, NetworksRequest>*,
                      std::promise<grpc::Status>*));
    MOCK_METHOD3(authenticate, void(const AuthenticateRequest*,
                                    grpc::ServerReaderWriterInterface<AuthenticateReply, AuthenticateRequest>*,
                                    std::promise<grpc::Status>*));

    template <typename Request, typename Reply>
    void set_promise_value(const Request*, grpc::ServerReaderWriterInterface<Reply, Request>*,
                           std::promise<grpc::Status>* status_promise)
    {
        status_promise->set_value(grpc::Status::OK);
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_TESTS_MOCK_DAEMON_H

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

    MOCK_METHOD3(create, void(const CreateRequest*, grpc::ServerWriter<CreateReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(launch, void(const LaunchRequest*, grpc::ServerWriter<LaunchReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(purge, void(const PurgeRequest*, grpc::ServerWriter<PurgeReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(find, void(const FindRequest* request, grpc::ServerWriter<FindReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(info, void(const InfoRequest*, grpc::ServerWriter<InfoReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(list, void(const ListRequest*, grpc::ServerWriter<ListReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(mount,
                 void(const MountRequest* request, grpc::ServerWriter<MountReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(recover, void(const RecoverRequest*, grpc::ServerWriter<RecoverReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(ssh_info, void(const SSHInfoRequest*, grpc::ServerWriter<SSHInfoReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(start, void(const StartRequest*, grpc::ServerWriter<StartReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(stop, void(const StopRequest*, grpc::ServerWriter<StopReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(suspend, void(const SuspendRequest*, grpc::ServerWriter<SuspendReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(restart, void(const RestartRequest*, grpc::ServerWriter<RestartReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(delet, void(const DeleteRequest*, grpc::ServerWriter<DeleteReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(umount, void(const UmountRequest*, grpc::ServerWriter<UmountReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(version, void(const VersionRequest*, grpc::ServerWriter<VersionReply>*, std::promise<grpc::Status>*));

    template <typename Request, typename Reply>
    void set_promise_value(const Request*, grpc::ServerWriter<Reply>*, std::promise<grpc::Status>* status_promise)
    {
        status_promise->set_value(grpc::Status::OK);
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_TESTS_MOCK_DAEMON_H

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

    MOCK_METHOD(void, shutdown_grpc_server, (), (override));
    MOCK_METHOD(void, create,
                (const CreateRequest*, (grpc::ServerReaderWriterInterface<CreateReply, CreateRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, launch,
                (const LaunchRequest*, (grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, purge,
                (const PurgeRequest*, (grpc::ServerReaderWriterInterface<PurgeReply, PurgeRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, find,
                (const FindRequest* request, (grpc::ServerReaderWriterInterface<FindReply, FindRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, info,
                (const InfoRequest*, (grpc::ServerReaderWriterInterface<InfoReply, InfoRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, list,
                (const ListRequest*, (grpc::ServerReaderWriterInterface<ListReply, ListRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, mount,
                (const MountRequest* request, (grpc::ServerReaderWriterInterface<MountReply, MountRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, recover,
                (const RecoverRequest*, (grpc::ServerReaderWriterInterface<RecoverReply, RecoverRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, ssh_info,
                (const SSHInfoRequest*, (grpc::ServerReaderWriterInterface<SSHInfoReply, SSHInfoRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, start,
                (const StartRequest*, (grpc::ServerReaderWriterInterface<StartReply, StartRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, stop,
                (const StopRequest*, (grpc::ServerReaderWriterInterface<StopReply, StopRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, suspend,
                (const SuspendRequest*, (grpc::ServerReaderWriterInterface<SuspendReply, SuspendRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, restart,
                (const RestartRequest*, (grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, delet,
                (const DeleteRequest*, (grpc::ServerReaderWriterInterface<DeleteReply, DeleteRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, umount,
                (const UmountRequest*, (grpc::ServerReaderWriterInterface<UmountReply, UmountRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, version,
                (const VersionRequest*, (grpc::ServerReaderWriterInterface<VersionReply, VersionRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, keys,
                (const KeysRequest*, (grpc::ServerReaderWriterInterface<KeysReply, KeysRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, get,
                (const GetRequest*, (grpc::ServerReaderWriterInterface<GetReply, GetRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, set,
                (const SetRequest*, (grpc::ServerReaderWriterInterface<SetReply, SetRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, networks,
                (const NetworksRequest*, (grpc::ServerReaderWriterInterface<NetworksReply, NetworksRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void, authenticate,
                (const AuthenticateRequest*,
                 (grpc::ServerReaderWriterInterface<AuthenticateReply, AuthenticateRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void,
                snapshot,
                (const SnapshotRequest*,
                 (grpc::ServerReaderWriterInterface<SnapshotReply, SnapshotRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void,
                restore,
                (const RestoreRequest*,
                 (grpc::ServerReaderWriterInterface<RestoreReply, RestoreRequest>*),
                 std::promise<grpc::Status>*),
                (override));
    MOCK_METHOD(void,
                clone,
                (const CloneRequest*,
                 (grpc::ServerReaderWriterInterface<CloneReply, CloneRequest>*),
                 std::promise<grpc::Status>*),
                (override));

    MOCK_METHOD(void,
                daemon_info,
                (const DaemonInfoRequest*,
                 (grpc::ServerReaderWriterInterface<DaemonInfoReply, DaemonInfoRequest>*),
                 std::promise<grpc::Status>*),
                (override));

    template <typename Request, typename Reply>
    void set_promise_value(const Request*, grpc::ServerReaderWriterInterface<Reply, Request>*,
                           std::promise<grpc::Status>* status_promise)
    {
        status_promise->set_value(grpc::Status::OK);
    }

    // The following functions are meant to test daemon's bridging functions.
    // This tests bridged interface addition.
    void test_add_bridged_interface(const std::string& instance_name,
                                    const VirtualMachine::ShPtr instance,
                                    const VMSpecs& specs = {})
    {
        vm_instance_specs.emplace(instance_name, specs);
        operative_instances.insert(std::make_pair(instance_name, instance));

        return add_bridged_interface(instance_name);
    }

    // This tests if the daemon is able to tell whether an instance has a bridged interface or not.
    bool test_is_bridged(const std::string& instance_name, const VMSpecs& specs)
    {
        vm_instance_specs.insert(std::make_pair(instance_name, specs));

        return is_bridged(instance_name);
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_TESTS_MOCK_DAEMON_H

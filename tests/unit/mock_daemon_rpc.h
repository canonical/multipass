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

#include <src/daemon/daemon_rpc.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <gmock/gmock.h>

namespace multipass::test
{

struct MockDaemonRpc : public DaemonRpc
{
    using DaemonRpc::DaemonRpc; // ctor

    MOCK_METHOD(grpc::Status,
                create,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<CreateReply, CreateRequest> * server)),
                (override)); // here only to ensure not called
    MOCK_METHOD(grpc::Status,
                launch,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<LaunchReply, LaunchRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                purge,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<PurgeReply, PurgeRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                remotes,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<RemotesReply, RemotesRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                images,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<ImagesReply, ImagesRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                info,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<InfoReply, InfoRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                list,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<ListReply, ListRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                snapshots,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<SnapshotsReply, SnapshotsRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                mount,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<MountReply, MountRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                recover,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<RecoverReply, RecoverRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                ssh_info,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<SSHInfoReply, SSHInfoRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                start,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<StartReply, StartRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                stop,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<StopReply, StopRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                suspend,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<SuspendReply, SuspendRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                restart,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<RestartReply, RestartRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                delet,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<DeleteReply, DeleteRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                umount,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<UmountReply, UmountRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                version,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<VersionReply, VersionRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                ping,
                (grpc::ServerContext * context,
                 const PingRequest* request,
                 PingReply* response),
                (override));
    MOCK_METHOD(grpc::Status,
                get,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<GetReply, GetRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                authenticate,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<AuthenticateReply, AuthenticateRequest> *
                  server)),
                (override));
    MOCK_METHOD(grpc::Status,
                snapshot,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<SnapshotReply, SnapshotRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                restore,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<RestoreReply, RestoreRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                clone,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<CloneReply, CloneRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                wait_ready,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<WaitReadyReply, WaitReadyRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                zones,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<ZonesReply, ZonesRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                zones_state,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<ZonesStateReply, ZonesStateRequest> * server)),
                (override));
};

} // namespace multipass::test

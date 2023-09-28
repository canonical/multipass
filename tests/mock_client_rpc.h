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

#ifndef MULTIPASS_MOCK_CLIENT_RPC_H
#define MULTIPASS_MOCK_CLIENT_RPC_H

#include "common.h"

#include <multipass/rpc/multipass.grpc.pb.h>

using namespace testing;

namespace multipass::test
{
template <class W, class R>
class MockClientReaderWriter : public grpc::ClientReaderWriterInterface<W, R>
{
public:
    MockClientReaderWriter()
    {
        EXPECT_CALL(*this, Read(_)).WillRepeatedly(Return(false));
        EXPECT_CALL(*this, Finish()).WillRepeatedly(Return(grpc::Status()));
    }

    MOCK_METHOD(grpc::Status, Finish, (), (override));
    MOCK_METHOD(bool, NextMessageSize, (uint32_t * sz), (override));
    MOCK_METHOD(bool, Read, (R * msg), (override));
    MOCK_METHOD(void, WaitForInitialMetadata, (), (override));
    MOCK_METHOD(bool, Write, (const W& msg, grpc::WriteOptions options), (override));
    MOCK_METHOD(bool, WritesDone, (), (override));
};

class MockRpcStub : public multipass::Rpc::StubInterface
{
public:
    MockRpcStub() = default;

    MOCK_METHOD(grpc::Status, ping,
                (grpc::ClientContext * context, const multipass::PingRequest& request, multipass::PingReply* response),
                (override));

    // originally private
    // NB: we're sort of relying on gRPC implementation here... but it's only for tests and we can update as needed
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::LaunchRequest, multipass::LaunchReply>*), createRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::LaunchRequest, multipass::LaunchReply>*),
                AsynccreateRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::LaunchRequest, multipass::LaunchReply>*),
                PrepareAsynccreateRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::LaunchRequest, multipass::LaunchReply>*), launchRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::LaunchRequest, multipass::LaunchReply>*),
                AsynclaunchRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::LaunchRequest, multipass::LaunchReply>*),
                PrepareAsynclaunchRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::PurgeRequest, multipass::PurgeReply>*), purgeRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::PurgeRequest, multipass::PurgeReply>*),
                AsyncpurgeRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::PurgeRequest, multipass::PurgeReply>*),
                PrepareAsyncpurgeRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::FindRequest, multipass::FindReply>*), findRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::FindRequest, multipass::FindReply>*), AsyncfindRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::FindRequest, multipass::FindReply>*),
                PrepareAsyncfindRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::InfoRequest, multipass::InfoReply>*), infoRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::InfoRequest, multipass::InfoReply>*), AsyncinfoRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::InfoRequest, multipass::InfoReply>*),
                PrepareAsyncinfoRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::ListRequest, multipass::ListReply>*), listRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::ListRequest, multipass::ListReply>*), AsynclistRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::ListRequest, multipass::ListReply>*),
                PrepareAsynclistRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::NetworksRequest, multipass::NetworksReply>*), networksRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::NetworksRequest, multipass::NetworksReply>*),
                AsyncnetworksRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::NetworksRequest, multipass::NetworksReply>*),
                PrepareAsyncnetworksRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::MountRequest, multipass::MountReply>*), mountRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::MountRequest, multipass::MountReply>*),
                AsyncmountRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::MountRequest, multipass::MountReply>*),
                PrepareAsyncmountRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<multipass::PingReply>*, AsyncpingRaw,
                (grpc::ClientContext * context, const multipass::PingRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<multipass::PingReply>*, PrepareAsyncpingRaw,
                (grpc::ClientContext * context, const multipass::PingRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::RecoverRequest, multipass::RecoverReply>*), recoverRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::RecoverRequest, multipass::RecoverReply>*),
                AsyncrecoverRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::RecoverRequest, multipass::RecoverReply>*),
                PrepareAsyncrecoverRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::SSHInfoRequest, multipass::SSHInfoReply>*), ssh_infoRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SSHInfoRequest, multipass::SSHInfoReply>*),
                Asyncssh_infoRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SSHInfoRequest, multipass::SSHInfoReply>*),
                PrepareAsyncssh_infoRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::StartRequest, multipass::StartReply>*), startRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::StartRequest, multipass::StartReply>*),
                AsyncstartRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::StartRequest, multipass::StartReply>*),
                PrepareAsyncstartRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::StopRequest, multipass::StopReply>*), stopRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::StopRequest, multipass::StopReply>*), AsyncstopRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::StopRequest, multipass::StopReply>*),
                PrepareAsyncstopRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::SuspendRequest, multipass::SuspendReply>*), suspendRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SuspendRequest, multipass::SuspendReply>*),
                AsyncsuspendRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SuspendRequest, multipass::SuspendReply>*),
                PrepareAsyncsuspendRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::RestartRequest, multipass::RestartReply>*), restartRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::RestartRequest, multipass::RestartReply>*),
                AsyncrestartRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::RestartRequest, multipass::RestartReply>*),
                PrepareAsyncrestartRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::DeleteRequest, multipass::DeleteReply>*), deletRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::DeleteRequest, multipass::DeleteReply>*),
                AsyncdeletRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::DeleteRequest, multipass::DeleteReply>*),
                PrepareAsyncdeletRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::UmountRequest, multipass::UmountReply>*), umountRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::UmountRequest, multipass::UmountReply>*),
                AsyncumountRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::UmountRequest, multipass::UmountReply>*),
                PrepareAsyncumountRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::VersionRequest, multipass::VersionReply>*), versionRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::VersionRequest, multipass::VersionReply>*),
                AsyncversionRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::VersionRequest, multipass::VersionReply>*),
                PrepareAsyncversionRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::GetRequest, multipass::GetReply>*), getRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::GetRequest, multipass::GetReply>*), AsyncgetRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::GetRequest, multipass::GetReply>*),
                PrepareAsyncgetRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::SetRequest, multipass::SetReply>*), setRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SetRequest, multipass::SetReply>*), AsyncsetRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SetRequest, multipass::SetReply>*),
                PrepareAsyncsetRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::KeysRequest, multipass::KeysReply>*), keysRaw,
                (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::KeysRequest, multipass::KeysReply>*), AsynckeysRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::KeysRequest, multipass::KeysReply>*),
                PrepareAsynckeysRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::AuthenticateRequest, multipass::AuthenticateReply>*),
                authenticateRaw, (grpc::ClientContext * context), (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::AuthenticateRequest, multipass::AuthenticateReply>*),
                AsyncauthenticateRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag),
                (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::AuthenticateRequest, multipass::AuthenticateReply>*),
                PrepareAsyncauthenticateRaw, (grpc::ClientContext * context, grpc::CompletionQueue* cq), (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::SnapshotRequest, multipass::SnapshotReply>*),
                snapshotRaw,
                (grpc::ClientContext * context),
                (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SnapshotRequest, multipass::SnapshotReply>*),
                AsyncsnapshotRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag),
                (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::SnapshotRequest, multipass::SnapshotReply>*),
                PrepareAsyncsnapshotRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::RestoreRequest, multipass::RestoreReply>*),
                restoreRaw,
                (grpc::ClientContext * context),
                (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::RestoreRequest, multipass::RestoreReply>*),
                AsyncrestoreRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag),
                (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::RestoreRequest, multipass::RestoreReply>*),
                PrepareAsyncrestoreRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq),
                (override));

    MOCK_METHOD((grpc::ClientReaderWriterInterface<multipass::CloneRequest, multipass::CloneReply>*),
                cloneRaw,
                (grpc::ClientContext * context),
                (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::CloneRequest, multipass::CloneReply>*),
                AsynccloneRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq, void* tag),
                (override));
    MOCK_METHOD((grpc::ClientAsyncReaderWriterInterface<multipass::CloneRequest, multipass::CloneReply>*),
                PrepareAsynccloneRaw,
                (grpc::ClientContext * context, grpc::CompletionQueue* cq),
                (override));
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_CLIENT_RPC_H

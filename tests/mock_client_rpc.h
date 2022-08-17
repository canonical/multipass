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

#ifndef MULTIPASS_MOCK_CLIENT_RPC_H
#define MULTIPASS_MOCK_CLIENT_RPC_H

#include "common.h"

#include <multipass/rpc/multipass.grpc.pb.h>

using namespace testing;

namespace multipass::test
{
template <class R>
class MockClientReader : public grpc::ClientReaderInterface<R>
{
public:
    MockClientReader()
    {
        EXPECT_CALL(*this, Read(_)).WillRepeatedly(Return(false));
        EXPECT_CALL(*this, Finish()).WillRepeatedly(Return(grpc::Status()));
    }

    MOCK_METHOD(grpc::Status, Finish, (), (override));
    MOCK_METHOD(bool, NextMessageSize, (uint32_t * sz), (override));
    MOCK_METHOD(bool, Read, (R * msg), (override));
    MOCK_METHOD(void, WaitForInitialMetadata, (), (override));
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
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::LaunchReply>*, createRaw,
                (grpc::ClientContext * context, const multipass::LaunchRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::LaunchReply>*, AsynccreateRaw,
                (grpc::ClientContext * context, const multipass::LaunchRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::LaunchReply>*, PrepareAsynccreateRaw,
                (grpc::ClientContext * context, const multipass::LaunchRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::LaunchReply>*, launchRaw,
                (grpc::ClientContext * context, const multipass::LaunchRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::LaunchReply>*, AsynclaunchRaw,
                (grpc::ClientContext * context, const multipass::LaunchRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::LaunchReply>*, PrepareAsynclaunchRaw,
                (grpc::ClientContext * context, const multipass::LaunchRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::PurgeReply>*, purgeRaw,
                (grpc::ClientContext * context, const multipass::PurgeRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::PurgeReply>*, AsyncpurgeRaw,
                (grpc::ClientContext * context, const multipass::PurgeRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::PurgeReply>*, PrepareAsyncpurgeRaw,
                (grpc::ClientContext * context, const multipass::PurgeRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::FindReply>*, findRaw,
                (grpc::ClientContext * context, const multipass::FindRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::FindReply>*, AsyncfindRaw,
                (grpc::ClientContext * context, const multipass::FindRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::FindReply>*, PrepareAsyncfindRaw,
                (grpc::ClientContext * context, const multipass::FindRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::InfoReply>*, infoRaw,
                (grpc::ClientContext * context, const multipass::InfoRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::InfoReply>*, AsyncinfoRaw,
                (grpc::ClientContext * context, const multipass::InfoRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::InfoReply>*, PrepareAsyncinfoRaw,
                (grpc::ClientContext * context, const multipass::InfoRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::ListReply>*, listRaw,
                (grpc::ClientContext * context, const multipass::ListRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::ListReply>*, AsynclistRaw,
                (grpc::ClientContext * context, const multipass::ListRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::ListReply>*, PrepareAsynclistRaw,
                (grpc::ClientContext * context, const multipass::ListRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::NetworksReply>*, networksRaw,
                (grpc::ClientContext * context, const multipass::NetworksRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::NetworksReply>*, AsyncnetworksRaw,
                (grpc::ClientContext * context, const multipass::NetworksRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::NetworksReply>*, PrepareAsyncnetworksRaw,
                (grpc::ClientContext * context, const multipass::NetworksRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::MountReply>*, mountRaw,
                (grpc::ClientContext * context, const multipass::MountRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::MountReply>*, AsyncmountRaw,
                (grpc::ClientContext * context, const multipass::MountRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::MountReply>*, PrepareAsyncmountRaw,
                (grpc::ClientContext * context, const multipass::MountRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<multipass::PingReply>*, AsyncpingRaw,
                (grpc::ClientContext * context, const multipass::PingRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientAsyncResponseReaderInterface<multipass::PingReply>*, PrepareAsyncpingRaw,
                (grpc::ClientContext * context, const multipass::PingRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::RecoverReply>*, recoverRaw,
                (grpc::ClientContext * context, const multipass::RecoverRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::RecoverReply>*, AsyncrecoverRaw,
                (grpc::ClientContext * context, const multipass::RecoverRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::RecoverReply>*, PrepareAsyncrecoverRaw,
                (grpc::ClientContext * context, const multipass::RecoverRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::SSHInfoReply>*, ssh_infoRaw,
                (grpc::ClientContext * context, const multipass::SSHInfoRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::SSHInfoReply>*, Asyncssh_infoRaw,
                (grpc::ClientContext * context, const multipass::SSHInfoRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::SSHInfoReply>*, PrepareAsyncssh_infoRaw,
                (grpc::ClientContext * context, const multipass::SSHInfoRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::StartReply>*, startRaw,
                (grpc::ClientContext * context, const multipass::StartRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::StartReply>*, AsyncstartRaw,
                (grpc::ClientContext * context, const multipass::StartRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::StartReply>*, PrepareAsyncstartRaw,
                (grpc::ClientContext * context, const multipass::StartRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::StopReply>*, stopRaw,
                (grpc::ClientContext * context, const multipass::StopRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::StopReply>*, AsyncstopRaw,
                (grpc::ClientContext * context, const multipass::StopRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::StopReply>*, PrepareAsyncstopRaw,
                (grpc::ClientContext * context, const multipass::StopRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::SuspendReply>*, suspendRaw,
                (grpc::ClientContext * context, const multipass::SuspendRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::SuspendReply>*, AsyncsuspendRaw,
                (grpc::ClientContext * context, const multipass::SuspendRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::SuspendReply>*, PrepareAsyncsuspendRaw,
                (grpc::ClientContext * context, const multipass::SuspendRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::RestartReply>*, restartRaw,
                (grpc::ClientContext * context, const multipass::RestartRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::RestartReply>*, AsyncrestartRaw,
                (grpc::ClientContext * context, const multipass::RestartRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::RestartReply>*, PrepareAsyncrestartRaw,
                (grpc::ClientContext * context, const multipass::RestartRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::DeleteReply>*, deletRaw,
                (grpc::ClientContext * context, const multipass::DeleteRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::DeleteReply>*, AsyncdeletRaw,
                (grpc::ClientContext * context, const multipass::DeleteRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::DeleteReply>*, PrepareAsyncdeletRaw,
                (grpc::ClientContext * context, const multipass::DeleteRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::UmountReply>*, umountRaw,
                (grpc::ClientContext * context, const multipass::UmountRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::UmountReply>*, AsyncumountRaw,
                (grpc::ClientContext * context, const multipass::UmountRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::UmountReply>*, PrepareAsyncumountRaw,
                (grpc::ClientContext * context, const multipass::UmountRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::VersionReply>*, versionRaw,
                (grpc::ClientContext * context, const multipass::VersionRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::VersionReply>*, AsyncversionRaw,
                (grpc::ClientContext * context, const multipass::VersionRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::VersionReply>*, PrepareAsyncversionRaw,
                (grpc::ClientContext * context, const multipass::VersionRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::GetReply>*, getRaw,
                (grpc::ClientContext * context, const multipass::GetRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::GetReply>*, AsyncgetRaw,
                (grpc::ClientContext * context, const multipass::GetRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::GetReply>*, PrepareAsyncgetRaw,
                (grpc::ClientContext * context, const multipass::GetRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::SetReply>*, setRaw,
                (grpc::ClientContext * context, const multipass::SetRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::SetReply>*, AsyncsetRaw,
                (grpc::ClientContext * context, const multipass::SetRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::SetReply>*, PrepareAsyncsetRaw,
                (grpc::ClientContext * context, const multipass::SetRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::KeysReply>*, keysRaw,
                (grpc::ClientContext * context, const multipass::KeysRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::KeysReply>*, AsynckeysRaw,
                (grpc::ClientContext * context, const multipass::KeysRequest& request, grpc::CompletionQueue* cq,
                 void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::KeysReply>*, PrepareAsynckeysRaw,
                (grpc::ClientContext * context, const multipass::KeysRequest& request, grpc::CompletionQueue* cq),
                (override));
    MOCK_METHOD(grpc::ClientReaderInterface<multipass::AuthenticateReply>*, authenticateRaw,
                (grpc::ClientContext * context, const multipass::AuthenticateRequest& request), (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::AuthenticateReply>*, AsyncauthenticateRaw,
                (grpc::ClientContext * context, const multipass::AuthenticateRequest& request,
                 grpc::CompletionQueue* cq, void* tag),
                (override));
    MOCK_METHOD(grpc::ClientAsyncReaderInterface<multipass::AuthenticateReply>*, PrepareAsyncauthenticateRaw,
                (grpc::ClientContext * context, const multipass::AuthenticateRequest& request,
                 grpc::CompletionQueue* cq),
                (override));
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_CLIENT_RPC_H

/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_STUB_DAEMON_RPC_H
#define MULTIPASS_STUB_DAEMON_RPC_H

#include <src/daemon/daemon_rpc.h>

namespace multipass
{
namespace test
{
struct StubDaemonRpc final : public DaemonRpc
{
    StubDaemonRpc(const std::string& server_address, RpcConnectionType type, const CertProvider& cert_provider,
                  const CertStore& client_cert_store)
        : DaemonRpc{server_address, type, cert_provider, client_cert_store}
    {
    }

    grpc::Status launch(grpc::ServerContext* context, const LaunchRequest* request,
                        grpc::ServerWriter<LaunchReply>* reply) override
    {
        return grpc::Status::OK;
    }

    grpc::Status purge(grpc::ServerContext* context, const PurgeRequest* request,
                       grpc::ServerWriter<PurgeReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status find(grpc::ServerContext* context, const FindRequest* request,
                      grpc::ServerWriter<FindReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status info(grpc::ServerContext* context, const InfoRequest* request,
                      grpc::ServerWriter<InfoReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status list(grpc::ServerContext* context, const ListRequest* request,
                      grpc::ServerWriter<ListReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status mount(grpc::ServerContext* context, const MountRequest* request,
                       grpc::ServerWriter<MountReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status recover(grpc::ServerContext* context, const RecoverRequest* request,
                         grpc::ServerWriter<RecoverReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request,
                          grpc::ServerWriter<SSHInfoReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status start(grpc::ServerContext* context, const StartRequest* request,
                       grpc::ServerWriter<StartReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status stop(grpc::ServerContext* context, const StopRequest* request,
                      grpc::ServerWriter<StopReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status suspend(grpc::ServerContext* context, const SuspendRequest* request,
                         grpc::ServerWriter<SuspendReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status restart(grpc::ServerContext* context, const RestartRequest* request,
                         grpc::ServerWriter<RestartReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status delet(grpc::ServerContext* context, const DeleteRequest* request,
                       grpc::ServerWriter<DeleteReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status umount(grpc::ServerContext* context, const UmountRequest* request,
                        grpc::ServerWriter<UmountReply>* response) override
    {
        return grpc::Status::OK;
    }

    grpc::Status version(grpc::ServerContext* context, const VersionRequest* request,
                         grpc::ServerWriter<VersionReply>* response) override
    {
        return grpc::Status::OK;
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_DAEMON_RPC_H

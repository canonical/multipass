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

#ifndef MULTIPASS_DAEMON_RPC_H
#define MULTIPASS_DAEMON_RPC_H

#include "daemon_config.h"

#include <multipass/cert_provider.h>
#include <multipass/disabled_copy_move.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/rpc_connection_type.h>

#include <grpcpp/grpcpp.h>

#include <QObject>

#include <future>
#include <memory>

namespace multipass
{
using CreateRequest = LaunchRequest;
using CreateReply = LaunchReply;
using CreateError = LaunchError;
using CreateProgress = LaunchProgress;

struct DaemonConfig;
class DaemonRpc : public QObject, public multipass::Rpc::Service, private DisabledCopyMove
{
    Q_OBJECT
public:
    DaemonRpc(const std::string& server_address, multipass::RpcConnectionType type, const CertProvider& cert_provider,
              CertStore* client_cert_store);

signals:
    void on_create(const CreateRequest* request, grpc::ServerWriter<CreateReply>* reply,
                   std::promise<grpc::Status>* status_promise);
    void on_launch(const LaunchRequest* request, grpc::ServerWriter<LaunchReply>* reply,
                   std::promise<grpc::Status>* status_promise);
    void on_purge(const PurgeRequest* request, grpc::ServerWriter<PurgeReply>* response,
                  std::promise<grpc::Status>* status_promise);
    void on_find(const FindRequest* request, grpc::ServerWriter<FindReply>* response,
                 std::promise<grpc::Status>* status_promise);
    void on_info(const InfoRequest* request, grpc::ServerWriter<InfoReply>* response,
                 std::promise<grpc::Status>* status_promise);
    void on_list(const ListRequest* request, grpc::ServerWriter<ListReply>* response,
                 std::promise<grpc::Status>* status_promise);
    void on_networks(const NetworksRequest* request, grpc::ServerWriter<NetworksReply>* response,
                     std::promise<grpc::Status>* status_promise);
    void on_mount(const MountRequest* request, grpc::ServerWriter<MountReply>* response,
                  std::promise<grpc::Status>* status_promise);
    void on_recover(const RecoverRequest* request, grpc::ServerWriter<RecoverReply>* response,
                    std::promise<grpc::Status>* status_promise);
    void on_ssh_info(const SSHInfoRequest* request, grpc::ServerWriter<SSHInfoReply>* response,
                     std::promise<grpc::Status>* status_promise);
    void on_start(const StartRequest* request, grpc::ServerWriter<StartReply>* response,
                  std::promise<grpc::Status>* status_promise);
    void on_stop(const StopRequest* request, grpc::ServerWriter<StopReply>* response,
                 std::promise<grpc::Status>* status_promise);
    void on_suspend(const SuspendRequest* request, grpc::ServerWriter<SuspendReply>* response,
                    std::promise<grpc::Status>* status_promise);
    void on_restart(const RestartRequest* request, grpc::ServerWriter<RestartReply>* response,
                    std::promise<grpc::Status>* status_promise);
    void on_delete(const DeleteRequest* request, grpc::ServerWriter<DeleteReply>* response,
                   std::promise<grpc::Status>* status_promise);
    void on_umount(const UmountRequest* request, grpc::ServerWriter<UmountReply>* response,
                   std::promise<grpc::Status>* status_promise);
    void on_version(const VersionRequest* request, grpc::ServerWriter<VersionReply>* response,
                    std::promise<grpc::Status>* status_promise);
    void on_get(const GetRequest* request, grpc::ServerWriter<GetReply>* response,
                std::promise<grpc::Status>* status_promise);
    void on_authenticate(const AuthenticateRequest* request, grpc::ServerWriter<AuthenticateReply>* response,
                         std::promise<grpc::Status>* status_promise);

private:
    template <typename OperationSignal>
    grpc::Status verify_client_and_dispatch_operation(OperationSignal signal, const std::string& client_cert);

    const std::string server_address;
    const RpcConnectionType connection_type;
    const std::unique_ptr<grpc::Server> server;
    CertStore* client_cert_store;

protected:
    grpc::Status create(grpc::ServerContext* context, const CreateRequest* request,
                        grpc::ServerWriter<CreateReply>* reply) override;
    grpc::Status launch(grpc::ServerContext* context, const LaunchRequest* request,
                        grpc::ServerWriter<LaunchReply>* reply) override;
    grpc::Status purge(grpc::ServerContext* context, const PurgeRequest* request,
                       grpc::ServerWriter<PurgeReply>* response) override;
    grpc::Status find(grpc::ServerContext* context, const FindRequest* request,
                      grpc::ServerWriter<FindReply>* response) override;
    grpc::Status info(grpc::ServerContext* context, const InfoRequest* request,
                      grpc::ServerWriter<InfoReply>* response) override;
    grpc::Status list(grpc::ServerContext* context, const ListRequest* request,
                      grpc::ServerWriter<ListReply>* response) override;
    grpc::Status networks(grpc::ServerContext* context, const NetworksRequest* request,
                          grpc::ServerWriter<NetworksReply>* response) override;
    grpc::Status mount(grpc::ServerContext* context, const MountRequest* request,
                       grpc::ServerWriter<MountReply>* response) override;
    grpc::Status recover(grpc::ServerContext* context, const RecoverRequest* request,
                         grpc::ServerWriter<RecoverReply>* response) override;
    grpc::Status ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request,
                          grpc::ServerWriter<SSHInfoReply>* response) override;
    grpc::Status start(grpc::ServerContext* context, const StartRequest* request,
                       grpc::ServerWriter<StartReply>* response) override;
    grpc::Status stop(grpc::ServerContext* context, const StopRequest* request,
                      grpc::ServerWriter<StopReply>* response) override;
    grpc::Status suspend(grpc::ServerContext* context, const SuspendRequest* request,
                         grpc::ServerWriter<SuspendReply>* response) override;
    grpc::Status restart(grpc::ServerContext* context, const RestartRequest* request,
                         grpc::ServerWriter<RestartReply>* response) override;
    grpc::Status delet(grpc::ServerContext* context, const DeleteRequest* request,
                       grpc::ServerWriter<DeleteReply>* response) override;
    grpc::Status umount(grpc::ServerContext* context, const UmountRequest* request,
                        grpc::ServerWriter<UmountReply>* response) override;
    grpc::Status version(grpc::ServerContext* context, const VersionRequest* request,
                         grpc::ServerWriter<VersionReply>* response) override;
    grpc::Status ping(grpc::ServerContext* context, const PingRequest* request, PingReply* response) override;
    grpc::Status get(grpc::ServerContext* context, const GetRequest* request,
                     grpc::ServerWriter<GetReply>* response) override;
    grpc::Status authenticate(grpc::ServerContext* context, const AuthenticateRequest* request,
                              grpc::ServerWriter<AuthenticateReply>* response) override;
};
} // namespace multipass
#endif // MULTIPASS_DAEMON_RPC_H

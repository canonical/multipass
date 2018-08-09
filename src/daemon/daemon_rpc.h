/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_DAEMON_RPC_H
#define MULTIPASS_DAEMON_RPC_H

#include "daemon_config.h"

#include <multipass/cert_provider.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/rpc_connection_type.h>

#include <grpcpp/grpcpp.h>

#include <QObject>
#include <memory>

namespace multipass
{
struct DaemonConfig;
class DaemonRpc : public QObject, public multipass::Rpc::Service
{
    Q_OBJECT
public:
    DaemonRpc(const std::string& server_address, multipass::RpcConnectionType type, const CertProvider& cert_provider,
              const CertStore& client_cert_store);
    DaemonRpc(const DaemonRpc&) = delete;
    DaemonRpc& operator=(const DaemonRpc&) = delete;

signals:
    // All these signals must be connected to with a BlockingQueuedConnection!!!
    grpc::Status on_launch(grpc::ServerContext* context, const LaunchRequest* request,
                           grpc::ServerWriter<LaunchReply>* reply);
    grpc::Status on_purge(grpc::ServerContext* context, const PurgeRequest* request, PurgeReply* response);
    grpc::Status on_find(grpc::ServerContext* context, const FindRequest* request, FindReply* response);
    grpc::Status on_info(grpc::ServerContext* context, const InfoRequest* request, InfoReply* response);
    grpc::Status on_list(grpc::ServerContext* context, const ListRequest* request, ListReply* response);
    grpc::Status on_mount(grpc::ServerContext* context, const MountRequest* request, MountReply* response);
    grpc::Status on_recover(grpc::ServerContext* context, const RecoverRequest* request, RecoverReply* response);
    grpc::Status on_ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request, SSHInfoReply* response);
    grpc::Status on_start(grpc::ServerContext* context, const StartRequest* request, StartReply* response);
    grpc::Status on_stop(grpc::ServerContext* context, const StopRequest* request, StopReply* response);
    grpc::Status on_delete(grpc::ServerContext* context, const DeleteRequest* request, DeleteReply* response);
    grpc::Status on_umount(grpc::ServerContext* context, const UmountRequest* request, UmountReply* response);
    grpc::Status on_version(grpc::ServerContext* context, const VersionRequest* request, VersionReply* response);
    grpc::Status on_register(grpc::ServerContext* context, const RegisterRequest* request, RegisterReply* response);

private:
    const std::unique_ptr<grpc::Server> server;

protected:
    grpc::Status launch(grpc::ServerContext* context, const LaunchRequest* request,
                        grpc::ServerWriter<LaunchReply>* reply) override;
    grpc::Status purge(grpc::ServerContext* context, const PurgeRequest* request, PurgeReply* response) override;
    grpc::Status find(grpc::ServerContext* context, const FindRequest* request, FindReply* response) override;
    grpc::Status info(grpc::ServerContext* context, const InfoRequest* request, InfoReply* response) override;
    grpc::Status list(grpc::ServerContext* context, const ListRequest* request, ListReply* response) override;
    grpc::Status mount(grpc::ServerContext* context, const MountRequest* request, MountReply* response) override;
    grpc::Status recover(grpc::ServerContext* context, const RecoverRequest* request, RecoverReply* response) override;
    grpc::Status ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request, SSHInfoReply* response) override;
    grpc::Status start(grpc::ServerContext* context, const StartRequest* request, StartReply* response) override;
    grpc::Status stop(grpc::ServerContext* context, const StopRequest* request, StopReply* response) override;
    grpc::Status delet(grpc::ServerContext* context, const DeleteRequest* request, DeleteReply* response) override;
    grpc::Status umount(grpc::ServerContext* context, const UmountRequest* request, UmountReply* response) override;
    grpc::Status version(grpc::ServerContext* context, const VersionRequest* request, VersionReply* response) override;
    grpc::Status ping(grpc::ServerContext* context, const PingRequest* request, PingReply* response) override;
    grpc::Status registr(grpc::ServerContext* context, const RegisterRequest* request,
                         RegisterReply* response) override;
};
} // namespace multipass
#endif // MULTIPASS_DAEMON_RPC_H

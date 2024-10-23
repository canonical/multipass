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

#ifndef MULTIPASS_DAEMON_RPC_H
#define MULTIPASS_DAEMON_RPC_H

#include "daemon_config.h"

#include <multipass/cert_provider.h>
#include <multipass/disabled_copy_move.h>
#include <multipass/rpc/multipass.grpc.pb.h>

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

// Skip running moc on this enum class since it fails parsing it
#ifndef Q_MOC_RUN
enum class ServerSocketType
{
    tcp,
    unix
};
#endif

struct DaemonConfig;
class DaemonRpc : public QObject, public multipass::Rpc::Service, private DisabledCopyMove
{
    Q_OBJECT
public:
    DaemonRpc(const std::string& server_address, const CertProvider& cert_provider, CertStore* client_cert_store);

    void shutdown_and_wait();

signals:
    void on_create(const CreateRequest* request, grpc::ServerReaderWriter<CreateReply, CreateRequest>* server,
                   std::promise<grpc::Status>* status_promise);
    void on_launch(const LaunchRequest* request, grpc::ServerReaderWriter<LaunchReply, LaunchRequest>* server,
                   std::promise<grpc::Status>* status_promise);
    void on_purge(const PurgeRequest* request, grpc::ServerReaderWriter<PurgeReply, PurgeRequest>* server,
                  std::promise<grpc::Status>* status_promise);
    void on_find(const FindRequest* request, grpc::ServerReaderWriter<FindReply, FindRequest>* server,
                 std::promise<grpc::Status>* status_promise);
    void on_info(const InfoRequest* request, grpc::ServerReaderWriter<InfoReply, InfoRequest>* server,
                 std::promise<grpc::Status>* status_promise);
    void on_list(const ListRequest* request, grpc::ServerReaderWriter<ListReply, ListRequest>* server,
                 std::promise<grpc::Status>* status_promise);
    void on_clone(const CloneRequest* request,
                  grpc::ServerReaderWriter<CloneReply, CloneRequest>* server,
                  std::promise<grpc::Status>* status_promise);
    void on_networks(const NetworksRequest* request, grpc::ServerReaderWriter<NetworksReply, NetworksRequest>* server,
                     std::promise<grpc::Status>* status_promise);
    void on_mount(const MountRequest* request, grpc::ServerReaderWriter<MountReply, MountRequest>* server,
                  std::promise<grpc::Status>* status_promise);
    void on_recover(const RecoverRequest* request, grpc::ServerReaderWriter<RecoverReply, RecoverRequest>* server,
                    std::promise<grpc::Status>* status_promise);
    void on_ssh_info(const SSHInfoRequest* request, grpc::ServerReaderWriter<SSHInfoReply, SSHInfoRequest>* server,
                     std::promise<grpc::Status>* status_promise);
    void on_start(const StartRequest* request, grpc::ServerReaderWriter<StartReply, StartRequest>* server,
                  std::promise<grpc::Status>* status_promise);
    void on_stop(const StopRequest* request, grpc::ServerReaderWriter<StopReply, StopRequest>* server,
                 std::promise<grpc::Status>* status_promise);
    void on_suspend(const SuspendRequest* request, grpc::ServerReaderWriter<SuspendReply, SuspendRequest>* server,
                    std::promise<grpc::Status>* status_promise);
    void on_restart(const RestartRequest* request, grpc::ServerReaderWriter<RestartReply, RestartRequest>* server,
                    std::promise<grpc::Status>* status_promise);
    void on_delete(const DeleteRequest* request, grpc::ServerReaderWriter<DeleteReply, DeleteRequest>* server,
                   std::promise<grpc::Status>* status_promise);
    void on_umount(const UmountRequest* request, grpc::ServerReaderWriter<UmountReply, UmountRequest>* server,
                   std::promise<grpc::Status>* status_promise);
    void on_version(const VersionRequest* request, grpc::ServerReaderWriter<VersionReply, VersionRequest>* server,
                    std::promise<grpc::Status>* status_promise);
    void on_get(const GetRequest* request, grpc::ServerReaderWriter<GetReply, GetRequest>* server,
                std::promise<grpc::Status>* status_promise);
    void on_set(const SetRequest* request, grpc::ServerReaderWriter<SetReply, SetRequest>* server,
                std::promise<grpc::Status>* status_promise);
    void on_keys(const KeysRequest* request, grpc::ServerReaderWriter<KeysReply, KeysRequest>* server,
                 std::promise<grpc::Status>* status_promise);
    void on_authenticate(const AuthenticateRequest* request,
                         grpc::ServerReaderWriter<AuthenticateReply, AuthenticateRequest>* server,
                         std::promise<grpc::Status>* status_promise);
    void on_snapshot(const SnapshotRequest* request,
                     grpc::ServerReaderWriter<SnapshotReply, SnapshotRequest>* server,
                     std::promise<grpc::Status>* status_promise);
    void on_restore(const RestoreRequest* request,
                    grpc::ServerReaderWriter<RestoreReply, RestoreRequest>* server,
                    std::promise<grpc::Status>* status_promise);
    void on_daemon_info(const DaemonInfoRequest* request,
                        grpc::ServerReaderWriter<DaemonInfoReply, DaemonInfoRequest>* server,
                        std::promise<grpc::Status>* status_promise);

private:
    template <typename OperationSignal>
    grpc::Status verify_client_and_dispatch_operation(OperationSignal signal, const std::string& client_cert);

    const std::string server_address;
    const std::unique_ptr<grpc::Server> server;
    const ServerSocketType server_socket_type;
    CertStore* client_cert_store;

protected:
    grpc::Status create(grpc::ServerContext* context,
                        grpc::ServerReaderWriter<CreateReply, CreateRequest>* server) override;
    grpc::Status launch(grpc::ServerContext* context,
                        grpc::ServerReaderWriter<LaunchReply, LaunchRequest>* server) override;
    grpc::Status purge(grpc::ServerContext* context,
                       grpc::ServerReaderWriter<PurgeReply, PurgeRequest>* server) override;
    grpc::Status find(grpc::ServerContext* context, grpc::ServerReaderWriter<FindReply, FindRequest>* server) override;
    grpc::Status info(grpc::ServerContext* context, grpc::ServerReaderWriter<InfoReply, InfoRequest>* server) override;
    grpc::Status list(grpc::ServerContext* context, grpc::ServerReaderWriter<ListReply, ListRequest>* server) override;
    grpc::Status clone(grpc::ServerContext* context,
                       grpc::ServerReaderWriter<CloneReply, CloneRequest>* server) override;
    grpc::Status networks(grpc::ServerContext* context,
                          grpc::ServerReaderWriter<NetworksReply, NetworksRequest>* server) override;
    grpc::Status mount(grpc::ServerContext* context,
                       grpc::ServerReaderWriter<MountReply, MountRequest>* server) override;
    grpc::Status recover(grpc::ServerContext* context,
                         grpc::ServerReaderWriter<RecoverReply, RecoverRequest>* server) override;
    grpc::Status ssh_info(grpc::ServerContext* context,
                          grpc::ServerReaderWriter<SSHInfoReply, SSHInfoRequest>* server) override;
    grpc::Status start(grpc::ServerContext* context,
                       grpc::ServerReaderWriter<StartReply, StartRequest>* server) override;
    grpc::Status stop(grpc::ServerContext* context, grpc::ServerReaderWriter<StopReply, StopRequest>* server) override;
    grpc::Status suspend(grpc::ServerContext* context,
                         grpc::ServerReaderWriter<SuspendReply, SuspendRequest>* server) override;
    grpc::Status restart(grpc::ServerContext* context,
                         grpc::ServerReaderWriter<RestartReply, RestartRequest>* server) override;
    grpc::Status delet(grpc::ServerContext* context,
                       grpc::ServerReaderWriter<DeleteReply, DeleteRequest>* server) override;
    grpc::Status umount(grpc::ServerContext* context,
                        grpc::ServerReaderWriter<UmountReply, UmountRequest>* server) override;
    grpc::Status version(grpc::ServerContext* context,
                         grpc::ServerReaderWriter<VersionReply, VersionRequest>* server) override;
    grpc::Status ping(grpc::ServerContext* context, const PingRequest* request, PingReply* reply) override;
    grpc::Status get(grpc::ServerContext* context, grpc::ServerReaderWriter<GetReply, GetRequest>* server) override;
    grpc::Status set(grpc::ServerContext* context, grpc::ServerReaderWriter<SetReply, SetRequest>* server) override;
    grpc::Status keys(grpc::ServerContext* context, grpc::ServerReaderWriter<KeysReply, KeysRequest>* server) override;
    grpc::Status authenticate(grpc::ServerContext* context,
                              grpc::ServerReaderWriter<AuthenticateReply, AuthenticateRequest>* server) override;
    grpc::Status snapshot(grpc::ServerContext* context,
                          grpc::ServerReaderWriter<SnapshotReply, SnapshotRequest>* server) override;
    grpc::Status restore(grpc::ServerContext* context,
                         grpc::ServerReaderWriter<RestoreReply, RestoreRequest>* server) override;
    grpc::Status daemon_info(grpc::ServerContext* context,
                             grpc::ServerReaderWriter<DaemonInfoReply, DaemonInfoRequest>* server) override;
};
} // namespace multipass
#endif // MULTIPASS_DAEMON_RPC_H

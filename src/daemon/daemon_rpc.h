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
#include <grpc++/grpc++.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <QObject>

#include <memory>

namespace multipass
{
struct DaemonConfig;
class DaemonRpc : public QObject, public multipass::Rpc::Service
{
    Q_OBJECT
public:
    DaemonRpc(const std::string& server_address, std::ostream& cout, std::ostream& cerr);

signals:
    // All these signals must be connected to with a BlockingQueuedConnection!!!
    grpc::Status on_create(grpc::ServerContext* context, const CreateRequest* request,
                           grpc::ServerWriter<CreateReply>* reply);
    grpc::Status on_empty_trash(grpc::ServerContext* context, const EmptyTrashRequest* request,
                                EmptyTrashReply* response);
    grpc::Status on_find(grpc::ServerContext* context, const FindRequest* request, FindReply* response);
    grpc::Status on_info(grpc::ServerContext* context, const InfoRequest* request, InfoReply* response);
    grpc::Status on_list(grpc::ServerContext* context, const ListRequest* request, ListReply* response);
    grpc::Status on_mount(grpc::ServerContext* context, const MountRequest* request, MountReply* response);
    grpc::Status on_recover(grpc::ServerContext* context, const RecoverRequest* request, RecoverReply* response);
    grpc::Status on_ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request, SSHInfoReply* response);
    grpc::Status on_start(grpc::ServerContext* context, const StartRequest* request, StartReply* response);
    grpc::Status on_stop(grpc::ServerContext* context, const StopRequest* request, StopReply* response);
    grpc::Status on_trash(grpc::ServerContext* context, const TrashRequest* request, TrashReply* response);
    grpc::Status on_umount(grpc::ServerContext* context, const UmountRequest* request, UmountReply* response);
    grpc::Status on_version(grpc::ServerContext* context, const VersionRequest* request, VersionReply* response);

private:
    const std::string server_address;
    const std::unique_ptr<grpc::Server> server;
    std::ostream& cout;
    std::ostream& cerr;

protected:
    grpc::Status create(grpc::ServerContext* context, const CreateRequest* request,
                        grpc::ServerWriter<CreateReply>* reply) override;
    grpc::Status empty_trash(grpc::ServerContext* context, const EmptyTrashRequest* request,
                             EmptyTrashReply* response) override;
    grpc::Status find(grpc::ServerContext* context, const FindRequest* request, FindReply* response) override;
    grpc::Status info(grpc::ServerContext* context, const InfoRequest* request, InfoReply* response) override;
    grpc::Status list(grpc::ServerContext* context, const ListRequest* request, ListReply* response) override;
    grpc::Status mount(grpc::ServerContext* context, const MountRequest* request, MountReply* response) override;
    grpc::Status recover(grpc::ServerContext* context, const RecoverRequest* request, RecoverReply* response) override;
    grpc::Status ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request, SSHInfoReply* response) override;
    grpc::Status start(grpc::ServerContext* context, const StartRequest* request, StartReply* response) override;
    grpc::Status stop(grpc::ServerContext* context, const StopRequest* request, StopReply* response) override;
    grpc::Status trash(grpc::ServerContext* context, const TrashRequest* request, TrashReply* response) override;
    grpc::Status umount(grpc::ServerContext* context, const UmountRequest* request, UmountReply* response) override;
    grpc::Status version(grpc::ServerContext* context, const VersionRequest* request, VersionReply* response) override;
    grpc::Status ping(grpc::ServerContext* context, const PingRequest* request, PingReply* response) override;

    DaemonRpc(const DaemonRpc&) = delete;
    DaemonRpc& operator=(const DaemonRpc&) = delete;
};
}
#endif // MULTIPASS_DAEMON_RPC_H

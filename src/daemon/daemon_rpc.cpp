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
 */

#include "daemon_rpc.h"
#include "daemon_config.h"

#include <multipass/logging/log.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>

#include <fmt/format.h>

#include <chrono>
#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "rpc";

void throw_if_server_exists(const std::string& address)
{
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    auto stub = mp::Rpc::NewStub(channel);

    grpc::ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
    context.set_deadline(deadline);

    mp::PingRequest request;
    mp::PingReply reply;
    auto status = stub->ping(&context, request, &reply);

    if (status.error_code() == grpc::StatusCode::OK)
        throw std::runtime_error(fmt::format("a multipass daemon already exists at {}", address));
}

auto make_server(const std::string& server_address, mp::RpcConnectionType conn_type,
                 const mp::CertProvider& cert_provider, const mp::CertStore& client_cert_store,
                 mp::Rpc::Service* service)
{
    throw_if_server_exists(server_address);
    grpc::ServerBuilder builder;

    std::shared_ptr<grpc::ServerCredentials> creds;
    if (conn_type == mp::RpcConnectionType::ssl)
    {
        grpc::SslServerCredentialsOptions opts(GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_BUT_DONT_VERIFY);
        opts.pem_key_cert_pairs.push_back({cert_provider.PEM_signing_key(), cert_provider.PEM_certificate()});
        opts.pem_root_certs = client_cert_store.PEM_cert_chain();
        creds = grpc::SslServerCredentials(opts);
    }
    else if (conn_type == mp::RpcConnectionType::insecure)
    {
        creds = grpc::InsecureServerCredentials();
    }
    else
    {
        throw std::invalid_argument("Unknown connection type");
    }

    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(service);

    std::unique_ptr<grpc::Server> server{builder.BuildAndStart()};
    if (server == nullptr)
        throw std::runtime_error(fmt::format("Failed to start multipass gRPC service at {}", server_address));

    return server;
}
} // namespace

mp::DaemonRpc::DaemonRpc(const std::string& server_address, mp::RpcConnectionType type,
                         const CertProvider& cert_provider, const CertStore& client_cert_store)
    : server_address{server_address}, server{make_server(server_address, type, cert_provider, client_cert_store, this)}
{
    std::string ssl_enabled = type == mp::RpcConnectionType::ssl ? "on" : "off";
    mpl::log(mpl::Level::info, category, fmt::format("gRPC listening on {}, SSL:{}", server_address, ssl_enabled));
}

grpc::Status mp::DaemonRpc::create(grpc::ServerContext* context, const CreateRequest* request,
                                   grpc::ServerWriter<CreateReply>* reply)
{
    return emit on_create(context, request, reply); // must block until slot returns
}

grpc::Status mp::DaemonRpc::launch(grpc::ServerContext* context, const LaunchRequest* request,
                                   grpc::ServerWriter<LaunchReply>* reply)
{
    return emit on_launch(context, request, reply); // must block until slot returns
}

grpc::Status mp::DaemonRpc::purge(grpc::ServerContext* context, const PurgeRequest* request,
                                  grpc::ServerWriter<PurgeReply>* response)
{
    return emit on_purge(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::find(grpc::ServerContext* context, const FindRequest* request,
                                 grpc::ServerWriter<FindReply>* response)
{
    return emit on_find(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::info(grpc::ServerContext* context, const InfoRequest* request,
                                 grpc::ServerWriter<InfoReply>* response)
{
    return emit on_info(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::list(grpc::ServerContext* context, const ListRequest* request,
                                 grpc::ServerWriter<ListReply>* response)
{
    return emit on_list(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::mount(grpc::ServerContext* context, const MountRequest* request,
                                  grpc::ServerWriter<MountReply>* response)
{
    return emit on_mount(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::recover(grpc::ServerContext* context, const RecoverRequest* request,
                                    grpc::ServerWriter<RecoverReply>* response)
{
    return emit on_recover(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request,
                                     grpc::ServerWriter<SSHInfoReply>* response)
{
    return emit on_ssh_info(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::start(grpc::ServerContext* context, const StartRequest* request,
                                  grpc::ServerWriter<StartReply>* response)
{
    return emit on_start(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::stop(grpc::ServerContext* context, const StopRequest* request,
                                 grpc::ServerWriter<StopReply>* response)
{
    return emit on_stop(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::suspend(grpc::ServerContext* context, const SuspendRequest* request,
                                    grpc::ServerWriter<SuspendReply>* response)
{
    return emit on_suspend(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::restart(grpc::ServerContext* context, const RestartRequest* request,
                                    grpc::ServerWriter<RestartReply>* response)
{
    return emit on_restart(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::delet(grpc::ServerContext* context, const DeleteRequest* request,
                                  grpc::ServerWriter<DeleteReply>* response)
{
    return emit on_delete(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::umount(grpc::ServerContext* context, const UmountRequest* request,
                                   grpc::ServerWriter<UmountReply>* response)
{
    return emit on_umount(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::version(grpc::ServerContext* context, const VersionRequest* request,
                                    grpc::ServerWriter<VersionReply>* response)
{
    return emit on_version(context, request, response); // must block until slot returns
}

grpc::Status mp::DaemonRpc::ping(grpc::ServerContext* context, const PingRequest* request, PingReply* response)
{
    return grpc::Status::OK;
}

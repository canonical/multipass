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

#include "daemon_rpc.h"
#include "daemon_config.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>

#include <chrono>
#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "rpc";

bool check_is_server_running(const std::string& address)
{
    auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    auto stub = mp::Rpc::NewStub(channel);

    grpc::ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(100); // should be enough...
    context.set_deadline(deadline);

    mp::PingRequest request;
    mp::PingReply reply;
    return stub->ping(&context, request, &reply).ok();
}

auto make_server(const std::string& server_address, mp::RpcConnectionType conn_type,
                 const mp::CertProvider& cert_provider, mp::Rpc::Service* service)
{
    grpc::ServerBuilder builder;

    std::shared_ptr<grpc::ServerCredentials> creds;
    if (conn_type == mp::RpcConnectionType::ssl)
    {
        grpc::SslServerCredentialsOptions opts(GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_BUT_DONT_VERIFY);
        opts.pem_key_cert_pairs.push_back({cert_provider.PEM_signing_key(), cert_provider.PEM_certificate()});
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
    {
        auto detail = check_is_server_running(server_address) ? " A multipass daemon is already running there." : "";
        throw std::runtime_error(
            fmt::format("Failed to start multipass gRPC service at {}.{}", server_address, detail));
    }

    return server;
}

template <typename OperationSignal>
grpc::Status emit_signal_and_wait_for_result(OperationSignal operation_signal)
{
    std::promise<grpc::Status> status_promise;
    auto status_future = status_promise.get_future();
    emit operation_signal(&status_promise);

    return status_future.get();
}

std::string client_cert_from(grpc::ServerContext* context)
{
    std::string client_cert;
    auto client_certs{context->auth_context()->FindPropertyValues("x509_pem_cert")};

    if (!client_certs.empty())
    {
        client_cert = client_certs.front().data();
    }

    return client_cert;
}
} // namespace

mp::DaemonRpc::DaemonRpc(const std::string& server_address, mp::RpcConnectionType type,
                         const CertProvider& cert_provider, CertStore* client_cert_store)
    : server_address{server_address},
      server{make_server(server_address, type, cert_provider, this)},
      client_cert_store{client_cert_store}
{
    MP_PLATFORM.set_server_permissions(server_address, client_cert_store->is_store_empty());

    std::string ssl_enabled = type == mp::RpcConnectionType::ssl ? "on" : "off";
    mpl::log(mpl::Level::info, category, fmt::format("gRPC listening on {}, SSL:{}", server_address, ssl_enabled));
}

grpc::Status mp::DaemonRpc::create(grpc::ServerContext* context, const CreateRequest* request,
                                   grpc::ServerWriter<CreateReply>* reply)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_create, this, request, reply, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::launch(grpc::ServerContext* context, const LaunchRequest* request,
                                   grpc::ServerWriter<LaunchReply>* reply)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_launch, this, request, reply, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::purge(grpc::ServerContext* context, const PurgeRequest* request,
                                  grpc::ServerWriter<PurgeReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_purge, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::find(grpc::ServerContext* context, const FindRequest* request,
                                 grpc::ServerWriter<FindReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_find, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::info(grpc::ServerContext* context, const InfoRequest* request,
                                 grpc::ServerWriter<InfoReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_info, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::list(grpc::ServerContext* context, const ListRequest* request,
                                 grpc::ServerWriter<ListReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_list, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::networks(grpc::ServerContext* context, const NetworksRequest* request,
                                     grpc::ServerWriter<NetworksReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_networks, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::mount(grpc::ServerContext* context, const MountRequest* request,
                                  grpc::ServerWriter<MountReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_mount, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::recover(grpc::ServerContext* context, const RecoverRequest* request,
                                    grpc::ServerWriter<RecoverReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_recover, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::ssh_info(grpc::ServerContext* context, const SSHInfoRequest* request,
                                     grpc::ServerWriter<SSHInfoReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_ssh_info, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::start(grpc::ServerContext* context, const StartRequest* request,
                                  grpc::ServerWriter<StartReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_start, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::stop(grpc::ServerContext* context, const StopRequest* request,
                                 grpc::ServerWriter<StopReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_stop, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::suspend(grpc::ServerContext* context, const SuspendRequest* request,
                                    grpc::ServerWriter<SuspendReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_suspend, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::restart(grpc::ServerContext* context, const RestartRequest* request,
                                    grpc::ServerWriter<RestartReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_restart, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::delet(grpc::ServerContext* context, const DeleteRequest* request,
                                  grpc::ServerWriter<DeleteReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_delete, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::umount(grpc::ServerContext* context, const UmountRequest* request,
                                   grpc::ServerWriter<UmountReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_umount, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::version(grpc::ServerContext* context, const VersionRequest* request,
                                    grpc::ServerWriter<VersionReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_version, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::ping(grpc::ServerContext* context, const PingRequest* request, PingReply* response)
{
    auto client_cert = client_cert_from(context);

    if (!client_cert.empty() && !client_cert_store->verify_cert(client_cert))
    {
        return grpc::Status{grpc::StatusCode::UNAUTHENTICATED, ""};
    }

    return grpc::Status::OK;
}

grpc::Status mp::DaemonRpc::get(grpc::ServerContext* context, const GetRequest* request,
                                grpc::ServerWriter<GetReply>* response)
{
    return verify_client_and_dispatch_operation(
        std::bind(&DaemonRpc::on_get, this, request, response, std::placeholders::_1), client_cert_from(context));
}

grpc::Status mp::DaemonRpc::authenticate(grpc::ServerContext* context, const AuthenticateRequest* request,
                                         grpc::ServerWriter<AuthenticateReply>* response)
{
    auto status = emit_signal_and_wait_for_result(
        std::bind(&DaemonRpc::on_authenticate, this, request, response, std::placeholders::_1));

    if (status.ok())
    {
        client_cert_store->add_cert(client_cert_from(context));
    }

    return status;
}

template <typename OperationSignal>
grpc::Status mp::DaemonRpc::verify_client_and_dispatch_operation(OperationSignal signal, const std::string& client_cert)
{
    if (!client_cert_store->verify_cert(client_cert))
    {
        return grpc::Status{grpc::StatusCode::UNAUTHENTICATED,
                            "The client is not registered with the Multipass service. Please use 'multipass register' "
                            "to authenticate the client."};
    }

    return emit_signal_and_wait_for_result(signal);
}

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

#include "daemon_rpc.h"

#include <multipass/daemon_rpc_context.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/utils.h>

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
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(100); // should be enough...
    context.set_deadline(deadline);

    mp::PingRequest request;
    mp::PingReply server;
    return stub->ping(&context, request, &server).ok();
}

auto make_server(const std::string& server_address,
                 const mp::CertProvider& cert_provider,
                 mp::Rpc::Service* service)
{
    grpc::ServerBuilder builder;

    std::shared_ptr<grpc::ServerCredentials> creds;
    grpc::SslServerCredentialsOptions opts(GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_BUT_DONT_VERIFY);
    opts.pem_key_cert_pairs.push_back(
        {cert_provider.PEM_signing_key(), cert_provider.PEM_certificate()});
    creds = grpc::SslServerCredentials(opts);

    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(service);

    std::unique_ptr<grpc::Server> server{builder.BuildAndStart()};
    if (server == nullptr)
    {
        auto detail = check_is_server_running(server_address)
                          ? " A multipass daemon is already running there."
                          : "";
        throw std::runtime_error(
            fmt::format("Failed to start multipass gRPC service at {}.{}", server_address, detail));
    }

    return server;
}

auto server_socket_type_for(const std::string& server_address)
{
    if (server_address.find("unix") == 0)
    {
        return mp::ServerSocketType::unix;
    }

    return mp::ServerSocketType::tcp;
}

template <typename T>
concept HasVerbosityLevel = requires(T t) { t.verbosity_level(); };

template <typename T, typename U, typename OperationSignal>
grpc::Status emit_signal_and_wait_for_result(OperationSignal operation_signal,
                                             grpc::ServerReaderWriterInterface<T, U>* server,
                                             U* request,
                                             mpl::MultiplexingLogger& mpx)
{
    auto level = [&request]() {
        if constexpr (HasVerbosityLevel<U>)
            return mpl::level_from(request->verbosity_level());
        else
            return mpl::Level::info;
    }();

    std::promise<grpc::Status> promise;
    auto future = promise.get_future();
    multipass::DaemonRpcContextImpl<T, U> ctx{promise, server, level, mpx};
    emit operation_signal(request,
                          static_cast<grpc::ServerReaderWriter<T, U>*>(server),
                          static_cast<multipass::DaemonRpcContext*>(&ctx));
    return future.get();
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

void handle_socket_restrictions(const std::string& server_address, const bool restricted)
{
    try
    {
        MP_PLATFORM.set_server_socket_restrictions(server_address, restricted);
    }
    catch (const std::exception& e)
    {
        mpl::error(category, "Fatal error: Cannot set server socket restrictions: {}", e.what());
        MP_UTILS.exit(EXIT_FAILURE);
    }
}

void accept_cert(mp::CertStore* client_cert_store,
                 const std::string& client_cert,
                 const std::string& server_address)
{
    client_cert_store->add_cert(client_cert);

    handle_socket_restrictions(server_address, false);
}
} // namespace

mp::DaemonRpc::DaemonRpc(const std::string& server_address,
                         const CertProvider& cert_provider,
                         CertStore* client_cert_store,
                         std::shared_ptr<logging::MultiplexingLogger> logger)
    : server_address{server_address},
      server{make_server(server_address, cert_provider, this)},
      server_socket_type{server_socket_type_for(server_address)},
      client_cert_store{client_cert_store},
      logger(logger)
{
    handle_socket_restrictions(server_address, client_cert_store->empty());

    mpl::info(category, "gRPC listening on {}", server_address);
}

void mp::DaemonRpc::shutdown_and_wait()
{
    server->Shutdown();
    server->Wait();
}

grpc::Status mp::DaemonRpc::create(grpc::ServerContext* context,
                                   grpc::ServerReaderWriter<CreateReply, CreateRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_create,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::launch(grpc::ServerContext* context,
                                   grpc::ServerReaderWriter<LaunchReply, LaunchRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_launch,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::purge(grpc::ServerContext* context,
                                  grpc::ServerReaderWriter<PurgeReply, PurgeRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_purge,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::find(grpc::ServerContext* context,
                                 grpc::ServerReaderWriter<FindReply, FindRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_find,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::info(grpc::ServerContext* context,
                                 grpc::ServerReaderWriter<InfoReply, InfoRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_info,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::list(grpc::ServerContext* context,
                                 grpc::ServerReaderWriter<ListReply, ListRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_list,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::clone(grpc::ServerContext* context,
                                  grpc::ServerReaderWriter<CloneReply, CloneRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_clone,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::networks(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<NetworksReply, NetworksRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_networks,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::mount(grpc::ServerContext* context,
                                  grpc::ServerReaderWriter<MountReply, MountRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_mount,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::recover(grpc::ServerContext* context,
                                    grpc::ServerReaderWriter<RecoverReply, RecoverRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_recover,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::ssh_info(grpc::ServerContext* context,
                                     grpc::ServerReaderWriter<SSHInfoReply, SSHInfoRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_ssh_info,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::start(grpc::ServerContext* context,
                                  grpc::ServerReaderWriter<StartReply, StartRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_start,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::stop(grpc::ServerContext* context,
                                 grpc::ServerReaderWriter<StopReply, StopRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_stop,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::suspend(grpc::ServerContext* context,
                                    grpc::ServerReaderWriter<SuspendReply, SuspendRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_suspend,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::restart(grpc::ServerContext* context,
                                    grpc::ServerReaderWriter<RestartReply, RestartRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_restart,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::delet(grpc::ServerContext* context,
                                  grpc::ServerReaderWriter<DeleteReply, DeleteRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_delete,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::umount(grpc::ServerContext* context,
                                   grpc::ServerReaderWriter<UmountReply, UmountRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_umount,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::version(grpc::ServerContext* context,
                                    grpc::ServerReaderWriter<VersionReply, VersionRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_version,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::ping(grpc::ServerContext* context,
                                 const PingRequest* request,
                                 PingReply* server)
{
    auto client_cert = client_cert_from(context);

    if (!client_cert.empty() && client_cert_store->verify_cert(client_cert))
    {
        return grpc::Status::OK;
    }

    return grpc::Status{grpc::StatusCode::UNAUTHENTICATED, ""};
}

grpc::Status mp::DaemonRpc::get(grpc::ServerContext* context,
                                grpc::ServerReaderWriter<GetReply, GetRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_get,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::authenticate(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<AuthenticateReply, AuthenticateRequest>* server)
{
    AuthenticateRequest request;
    server->Read(&request);

    auto status = emit_signal_and_wait_for_result(std::bind(&DaemonRpc::on_authenticate,
                                                            this,
                                                            std::placeholders::_1,
                                                            std::placeholders::_2,
                                                            std::placeholders::_3),
                                                  server,
                                                  &request,
                                                  *logger);

    if (status.ok())
    {
        try
        {
            accept_cert(client_cert_store, client_cert_from(context), server_address);
        }
        catch (const std::exception& e)
        {
            return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
        }
    }

    return status;
}

grpc::Status mp::DaemonRpc::set(grpc::ServerContext* context,
                                grpc::ServerReaderWriter<SetReply, SetRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_set,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::keys(grpc::ServerContext* context,
                                 grpc::ServerReaderWriter<KeysReply, KeysRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_keys,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::snapshot(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<SnapshotReply, SnapshotRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_snapshot,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::restore(grpc::ServerContext* context,
                                    grpc::ServerReaderWriter<RestoreReply, RestoreRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_restore,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::daemon_info(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<DaemonInfoReply, DaemonInfoRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_daemon_info,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

grpc::Status mp::DaemonRpc::wait_ready(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<WaitReadyReply, WaitReadyRequest>* server)
{
    return verify_client_and_dispatch_operation(std::bind(&DaemonRpc::on_wait_ready,
                                                          this,
                                                          std::placeholders::_1,
                                                          std::placeholders::_2,
                                                          std::placeholders::_3),
                                                client_cert_from(context),
                                                server);
}

template <typename T, typename U, typename OperationSignal>
grpc::Status
mp::DaemonRpc::verify_client_and_dispatch_operation(OperationSignal signal,
                                                    const std::string& client_cert,
                                                    grpc::ServerReaderWriterInterface<T, U>* server)
{
    U request{};
    server->Read(&request);
    if (server_socket_type == mp::ServerSocketType::unix && client_cert_store->empty())
    {
        try
        {
            accept_cert(client_cert_store, client_cert, server_address);
        }
        catch (const std::exception& e)
        {
            return grpc::Status{grpc::StatusCode::INTERNAL, e.what()};
        }
    }
    else if (!client_cert_store->verify_cert(client_cert))
    {
        return grpc::Status{
            grpc::StatusCode::UNAUTHENTICATED,
            "The user is not authenticated with the Multipass service.\n\n"
            "Please authenticate before proceeding (e.g. via 'multipass authenticate'). Note that "
            "you first need an authenticated user to set and provide you with a trusted passphrase "
            "(e.g. via 'multipass set local.passphrase')."};
    }

    return emit_signal_and_wait_for_result(signal, server, &request, *logger);
}

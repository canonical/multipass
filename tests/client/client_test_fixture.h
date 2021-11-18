/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_CLIENT_TEST_FIXTURE_H
#define MULTIPASS_CLIENT_TEST_FIXTURE_H

#include <tests/common.h>
#include <tests/stub_cert_store.h>
#include <tests/stub_certprovider.h>
#include <tests/stub_terminal.h>

#include <src/client/cli/client.h>
#include <src/daemon/daemon_rpc.h>

using namespace testing;

static std::stringstream trash_stream; // this may have contents (that we don't care about)

namespace multipass
{
namespace test
{
struct MockDaemonRpc : public DaemonRpc
{
    using DaemonRpc::DaemonRpc; // ctor

    MOCK_METHOD3(create, grpc::Status(grpc::ServerContext* context, const CreateRequest* request,
                                      grpc::ServerWriter<CreateReply>* reply)); // here only to ensure not called
    MOCK_METHOD3(launch, grpc::Status(grpc::ServerContext* context, const LaunchRequest* request,
                                      grpc::ServerWriter<LaunchReply>* reply));
    MOCK_METHOD3(purge, grpc::Status(grpc::ServerContext* context, const PurgeRequest* request,
                                     grpc::ServerWriter<PurgeReply>* response));
    MOCK_METHOD3(find, grpc::Status(grpc::ServerContext* context, const FindRequest* request,
                                    grpc::ServerWriter<FindReply>* response));
    MOCK_METHOD3(info, grpc::Status(grpc::ServerContext* context, const InfoRequest* request,
                                    grpc::ServerWriter<InfoReply>* response));
    MOCK_METHOD3(list, grpc::Status(grpc::ServerContext* context, const ListRequest* request,
                                    grpc::ServerWriter<ListReply>* response));
    MOCK_METHOD3(mount, grpc::Status(grpc::ServerContext* context, const MountRequest* request,
                                     grpc::ServerWriter<MountReply>* response));
    MOCK_METHOD3(recover, grpc::Status(grpc::ServerContext* context, const RecoverRequest* request,
                                       grpc::ServerWriter<RecoverReply>* response));
    MOCK_METHOD3(ssh_info, grpc::Status(grpc::ServerContext* context, const SSHInfoRequest* request,
                                        grpc::ServerWriter<SSHInfoReply>* response));
    MOCK_METHOD3(start, grpc::Status(grpc::ServerContext* context, const StartRequest* request,
                                     grpc::ServerWriter<StartReply>* response));
    MOCK_METHOD3(stop, grpc::Status(grpc::ServerContext* context, const StopRequest* request,
                                    grpc::ServerWriter<StopReply>* response));
    MOCK_METHOD3(suspend, grpc::Status(grpc::ServerContext* context, const SuspendRequest* request,
                                       grpc::ServerWriter<SuspendReply>* response));
    MOCK_METHOD3(restart, grpc::Status(grpc::ServerContext* context, const RestartRequest* request,
                                       grpc::ServerWriter<RestartReply>* response));
    MOCK_METHOD3(delet, grpc::Status(grpc::ServerContext* context, const DeleteRequest* request,
                                     grpc::ServerWriter<DeleteReply>* response));
    MOCK_METHOD3(umount, grpc::Status(grpc::ServerContext* context, const UmountRequest* request,
                                      grpc::ServerWriter<UmountReply>* response));
    MOCK_METHOD3(version, grpc::Status(grpc::ServerContext* context, const VersionRequest* request,
                                       grpc::ServerWriter<VersionReply>* response));
    MOCK_METHOD3(ping, grpc::Status(grpc::ServerContext* context, const PingRequest* request, PingReply* response));
    MOCK_METHOD3(get, grpc::Status(grpc::ServerContext* context, const GetRequest* request,
                                   grpc::ServerWriter<GetReply>* response));
};

struct ClientTestFixture : public Test
{
    void TearDown() override
    {
        Mock::VerifyAndClearExpectations(&mock_daemon); /* We got away without this before because, being a strict mock
                                                           every call to mock_daemon had to be explicitly "expected".
                                                           Being the best match for incoming calls, each expectation
                                                           took precedence over the previous ones, preventing them from
                                                           being saturated inadvertently */
    }

    int send_command(const std::vector<std::string>& command, std::ostream& cout = trash_stream,
                     std::ostream& cerr = trash_stream, std::istream& cin = trash_stream)
    {
        StubTerminal term(cout, cerr, cin);
        ClientConfig client_config{server_address, RpcConnectionType::insecure, std::make_unique<StubCertProvider>(),
                                   &term};
        Client client{client_config};
        QStringList args = QStringList() << "multipass_test";

        for (const auto& arg : command)
        {
            args << QString::fromStdString(arg);
        }
        return client.run(args);
    }

#ifdef MULTIPASS_PLATFORM_WINDOWS
    std::string server_address{"localhost:50051"};
#else
    std::string server_address{"unix:/tmp/test-multipassd.socket"};
#endif
    StubCertProvider cert_provider;
    StubCertStore cert_store;
    StrictMock<MockDaemonRpc> mock_daemon{server_address, RpcConnectionType::insecure, cert_provider,
                                          cert_store}; // strict to fail on unexpected calls and play well with sharing
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_CLIENT_TEST_FIXTURE_H

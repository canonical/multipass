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

#include "daemon_test_fixture.h"
#include "mock_cert_provider.h"
#include "mock_cert_store.h"
#include "mock_daemon.h"
#include "mock_platform.h"

#include <src/daemon/daemon_rpc.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
constexpr auto key_data = "-----BEGIN PRIVATE KEY-----\n"
                          "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgsSAz5ggzrLjai0I/\n"
                          "F0hYg5oG/shpXJiBQtJdBCG3lUShRANCAAQAFGNAqq7c5IMDeQ/cV4+Emogmkfpb\n"
                          "TLSPfXgXVLHRsvL04xUAkqGpL+eyGFVE6dqaJ7sAPJJwlVj1xD0r5DX5\n"
                          "-----END PRIVATE KEY-----\n";

constexpr auto cert_data = "-----BEGIN CERTIFICATE-----\n"
                           "MIIBUjCB+AIBKjAKBggqhkjOPQQDAjA1MQswCQYDVQQGEwJDQTESMBAGA1UECgwJ\n"
                           "Q2Fub25pY2FsMRIwEAYDVQQDDAlsb2NhbGhvc3QwHhcNMTgwNjIxMTM0MjI5WhcN\n"
                           "MTkwNjIxMTM0MjI5WjA1MQswCQYDVQQGEwJDQTESMBAGA1UECgwJQ2Fub25pY2Fs\n"
                           "MRIwEAYDVQQDDAlsb2NhbGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQA\n"
                           "FGNAqq7c5IMDeQ/cV4+EmogmkfpbTLSPfXgXVLHRsvL04xUAkqGpL+eyGFVE6dqa\n"
                           "J7sAPJJwlVj1xD0r5DX5MAoGCCqGSM49BAMCA0kAMEYCIQCvI0PYv9f201fbe4LP\n"
                           "BowTeYWSqMQtLNjvZgd++AAGhgIhALNPW+NRSKCXwadiIFgpbjPInLPqXPskLWSc\n"
                           "aXByaQyt\n"
                           "-----END CERTIFICATE-----\n";

struct TestDaemonRpc : public mpt::DaemonTestFixture
{
    TestDaemonRpc()
    {
        EXPECT_CALL(*mock_cert_provider, PEM_certificate()).WillOnce(Return(cert_data));
        EXPECT_CALL(*mock_cert_provider, PEM_signing_key()).WillOnce(Return(key_data));
    }

    mp::Rpc::Stub make_secure_stub()
    {
        auto opts = grpc::SslCredentialsOptions();
        opts.server_certificate_request = GRPC_SSL_REQUEST_SERVER_CERTIFICATE_BUT_DONT_VERIFY;
        opts.pem_cert_chain = mpt::client_cert;
        opts.pem_private_key = mpt::client_key;

        auto channel = grpc::CreateChannel(server_address, grpc::SslCredentials(opts));

        return mp::Rpc::Stub(channel);
    }

    mpt::MockDaemon make_secure_server()
    {
        config_builder.connection_type = mp::RpcConnectionType::ssl;
        config_builder.cert_provider = std::move(mock_cert_provider);
        config_builder.client_cert_store = std::move(mock_cert_store);

        return mpt::MockDaemon(config_builder.build());
    }

    std::unique_ptr<mpt::MockCertProvider> mock_cert_provider{std::make_unique<mpt::MockCertProvider>()};
    std::unique_ptr<mpt::MockCertStore> mock_cert_store{std::make_unique<mpt::MockCertStore>()};
    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject()};
    mpt::MockPlatform* mock_platform = platform_attr.first;
};
} // namespace

TEST_F(TestDaemonRpc, setsRestrictedPermissionsWhenNoCerts)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, true)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).WillOnce(Return(true));

    mpt::MockDaemon daemon{make_secure_server()};
}

TEST_F(TestDaemonRpc, setsUnrestrictedPermissionsWhenCertAlreadyExists)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, false)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).WillOnce(Return(false));

    mpt::MockDaemon daemon{make_secure_server()};
}

TEST_F(TestDaemonRpc, authenticateCompletesSuccesfully)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, true)).Times(1);
    EXPECT_CALL(*mock_platform, set_server_permissions(_, false)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).WillOnce(Return(true)).WillOnce(Return(true));
    EXPECT_CALL(*mock_cert_store, add_cert(StrEq(mpt::client_cert))).Times(1);

    mpt::MockDaemon daemon{make_secure_server()};
    EXPECT_CALL(daemon, authenticate(_, _, _)).WillOnce([](auto, auto, auto* status_promise) {
        status_promise->set_value(grpc::Status::OK);
    });

    send_command({"register", "foo"});
}

TEST_F(TestDaemonRpc, authenticateFailsSkipsCertImportCalls)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, true)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).WillOnce(Return(true));
    EXPECT_CALL(*mock_cert_store, add_cert(_)).Times(0);

    mpt::MockDaemon daemon{make_secure_server()};
    EXPECT_CALL(daemon, authenticate(_, _, _)).WillOnce([](auto, auto, auto* status_promise) {
        status_promise->set_value(grpc::Status(grpc::StatusCode::INTERNAL, ""));
    });

    send_command({"register", "foo"});
}

TEST_F(TestDaemonRpc, pingReturnsOkWhenCertIsVerified)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, false)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).WillOnce(Return(false));
    EXPECT_CALL(*mock_cert_store, verify_cert(StrEq(mpt::client_cert))).WillOnce(Return(true));

    mpt::MockDaemon daemon{make_secure_server()};
    mp::Rpc::Stub stub{make_secure_stub()};

    grpc::ClientContext context;
    mp::PingRequest request;
    mp::PingReply reply;

    EXPECT_TRUE(stub.ping(&context, request, &reply).ok());
}

TEST_F(TestDaemonRpc, pingReturnsUnauthenticatedWhenCertIsNotVerified)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, false)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).WillOnce(Return(false));
    EXPECT_CALL(*mock_cert_store, verify_cert(StrEq(mpt::client_cert))).WillOnce(Return(false));

    mpt::MockDaemon daemon{make_secure_server()};
    mp::Rpc::Stub stub{make_secure_stub()};

    grpc::ClientContext context;
    mp::PingRequest request;
    mp::PingReply reply;

    EXPECT_EQ(stub.ping(&context, request, &reply).error_code(), grpc::StatusCode::UNAUTHENTICATED);
}

// The following 'list' command tests are for testing the authentication of an arbirary command in DaemonRpc
TEST_F(TestDaemonRpc, listCertExistsCompletesSuccesfully)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, false)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).Times(2).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_cert_store, verify_cert(StrEq(mpt::client_cert))).WillOnce(Return(true));

    mpt::MockDaemon daemon{make_secure_server()};
    EXPECT_CALL(daemon, list(_, _, _)).WillOnce([](auto, auto, auto* status_promise) {
        status_promise->set_value(grpc::Status::OK);
    });

    send_command({"list"});
}

TEST_F(TestDaemonRpc, listNoCertsExistWillVerifyAndComplete)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, true)).Times(1);
    EXPECT_CALL(*mock_platform, set_server_permissions(_, false)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_cert_store, verify_cert(StrEq(mpt::client_cert))).WillOnce(Return(true));

    mpt::MockDaemon daemon{make_secure_server()};
    EXPECT_CALL(daemon, list(_, _, _)).WillOnce([](auto, auto, auto* status_promise) {
        status_promise->set_value(grpc::Status::OK);
    });

    send_command({"list"});
}

TEST_F(TestDaemonRpc, listCertNotVerifiedHasError)
{
    EXPECT_CALL(*mock_platform, set_server_permissions(_, false)).Times(1);

    EXPECT_CALL(*mock_cert_store, is_store_empty()).Times(2).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_cert_store, verify_cert(StrEq(mpt::client_cert))).WillOnce(Return(false));

    mpt::MockDaemon daemon{make_secure_server()};

    std::stringstream stream;

    send_command({"list"}, trash_stream, stream);

    EXPECT_THAT(stream.str(), AllOf(HasSubstr("The client is not registered with the Multipass service."),
                                    HasSubstr("Please use 'multipass register' to authenticate the client.")));
}

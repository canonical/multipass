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

#include "common.h"
#include "file_operations.h"
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_utils.h"
#include "temp_dir.h"

#include <multipass/client_cert_store.h>
#include <multipass/constants.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
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

constexpr auto cert2_data = "-----BEGIN CERTIFICATE-----\n"
                            "MIIBizCCATECBBv4mFwwCgYIKoZIzj0EAwIwUDELMAkGA1UEBhMCVVMxEjAQBgNV\n"
                            "BAoMCUNhbm9uaWNhbDEtMCsGA1UEAwwkNThhZGNkMjQtNDJmMi00ZjI0LWExYTYt\n"
                            "ODk5MDY3ZTdkODhkMB4XDTIxMTEwOTE1MDk0NloXDTIyMTEwOTE1MDk0NlowUDEL\n"
                            "MAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEtMCsGA1UEAwwkNThhZGNk\n"
                            "MjQtNDJmMi00ZjI0LWExYTYtODk5MDY3ZTdkODhkMFkwEwYHKoZIzj0CAQYIKoZI\n"
                            "zj0DAQcDQgAEqybAYAPImXZX5tZSJi6oyvkt4S/sZbk+mkoeg8t9G2lLbMDSG6W1\n"
                            "yN7oKVc/A6QJ4SO7FmTAr0ruAYQkBo65czAKBggqhkjOPQQDAgNIADBFAiEA/J34\n"
                            "z4dITtBKaWWUVpGt9Ih2ZCzwinvAh3w3eUaI5hECIFiT1JaL6QRa3holvTRpDm5O\n"
                            "5ZaxnIFvH2NZ/dCmFWQT\n"
                            "-----END CERTIFICATE-----\n";

struct ClientCertStore : public testing::Test
{
    ClientCertStore()
    {
        cert_dir = MP_UTILS.make_dir(temp_dir.path(), mp::authenticated_certs_dir,
                                     QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    mpt::TempDir temp_dir;
    mp::Path cert_dir;
};
} // namespace

TEST_F(ClientCertStore, returns_empty_chain_if_no_certificate_found)
{
    mp::ClientCertStore cert_store{temp_dir.path()};

    auto cert_chain = cert_store.PEM_cert_chain();
    EXPECT_TRUE(cert_chain.empty());
}

TEST_F(ClientCertStore, returns_persisted_certificate_chain)
{
    mp::ClientCertStore cert_store{temp_dir.path()};

    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    auto cert_chain = cert_store.PEM_cert_chain();
    EXPECT_THAT(cert_chain, StrEq(cert_data));
}

TEST_F(ClientCertStore, add_cert_throws_on_invalid_data)
{
    mp::ClientCertStore cert_store{temp_dir.path()};

    MP_EXPECT_THROW_THAT(cert_store.add_cert("not a certificate"), std::runtime_error,
                         mpt::match_what(StrEq("invalid certificate data")));
}

TEST_F(ClientCertStore, add_cert_stores_certificate)
{
    mp::ClientCertStore cert_store{temp_dir.path()};
    EXPECT_NO_THROW(cert_store.add_cert(cert_data));

    const auto content = cert_store.PEM_cert_chain();
    EXPECT_THAT(content, StrEq(cert_data));
}

TEST_F(ClientCertStore, verifyCertEmptyStoreReturnsFalse)
{
    mp::ClientCertStore cert_store{temp_dir.path()};

    ASSERT_TRUE(cert_store.PEM_cert_chain().empty());

    EXPECT_FALSE(cert_store.verify_cert(cert_data));

    const auto content = cert_store.PEM_cert_chain();

    EXPECT_TRUE(content.empty());
}

TEST_F(ClientCertStore, verifyCertInStoreReturnsTrue)
{
    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    mp::ClientCertStore cert_store{temp_dir.path()};

    ASSERT_FALSE(cert_store.PEM_cert_chain().empty());

    EXPECT_TRUE(cert_store.verify_cert(cert_data));
}

TEST_F(ClientCertStore, addCertAlreadyExistingDoesNotAddAgain)
{
    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    mp::ClientCertStore cert_store{temp_dir.path()};

    ASSERT_FALSE(cert_store.PEM_cert_chain().empty());

    EXPECT_NO_THROW(cert_store.add_cert(cert_data));

    const auto content = cert_store.PEM_cert_chain();

    EXPECT_EQ(content, cert_data);
}

TEST_F(ClientCertStore, addCertWithExistingCertPersistsCerts)
{
    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    mp::ClientCertStore cert_store{temp_dir.path()};

    ASSERT_FALSE(cert_store.PEM_cert_chain().empty());

    cert_store.add_cert(cert2_data);

    auto all_certs = std::string(cert_data) + std::string(cert2_data);

    const auto content = cert_store.PEM_cert_chain();

    EXPECT_EQ(content, all_certs);
}

TEST_F(ClientCertStore, storeEmptyReturnsTrueWhenNoCerts)
{
    mp::ClientCertStore cert_store{temp_dir.path()};

    EXPECT_TRUE(cert_store.empty());
}

TEST_F(ClientCertStore, storeEmptyReturnsFalseWhenCertExists)
{
    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    mp::ClientCertStore cert_store{temp_dir.path()};

    EXPECT_FALSE(cert_store.empty());
}

TEST_F(ClientCertStore, openingFileForWritingFailsAndThrows)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(false));

    mp::ClientCertStore cert_store{temp_dir.path()};

    MP_EXPECT_THROW_THAT(cert_store.add_cert(cert_data), std::runtime_error,
                         mpt::match_what(StrEq("failed to create file to store certificate")));
}

TEST_F(ClientCertStore, writingFileFailsAndThrows)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce([](QFileDevice& file, QIODevice::OpenMode mode) {
        return file.open(mode);
    });
    EXPECT_CALL(*mock_file_ops, write(_, _)).WillOnce(Return(-1));
    EXPECT_CALL(*mock_file_ops, commit).WillOnce(Return(false));

    mp::ClientCertStore cert_store{temp_dir.path()};

    MP_EXPECT_THROW_THAT(cert_store.add_cert(cert_data), std::runtime_error,
                         mpt::match_what(StrEq("failed to write certificate")));
}

/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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
#include "temp_dir.h"

#include <multipass/client_cert_store.h>
#include <multipass/utils.h>

namespace mp = multipass;
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

struct ClientCertStore : public testing::Test
{
    ClientCertStore()
    {
        cert_dir = mp::utils::make_dir(temp_dir.path(), "test-cert-store");
    }
    mpt::TempDir temp_dir;
    mp::Path cert_dir;
};
} // namespace

TEST_F(ClientCertStore, returns_empty_chain_if_no_certificate_found)
{
    mp::ClientCertStore cert_store{cert_dir};

    auto cert_chain = cert_store.PEM_cert_chain();
    EXPECT_TRUE(cert_chain.empty());
}

TEST_F(ClientCertStore, returns_persisted_certificate_chain)
{
    mp::ClientCertStore cert_store{cert_dir};

    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    auto cert_chain = cert_store.PEM_cert_chain();
    EXPECT_THAT(cert_chain, StrEq(cert_data));
}

TEST_F(ClientCertStore, add_cert_throws_on_invalid_data)
{
    mp::ClientCertStore cert_store{cert_dir};

    EXPECT_THROW(cert_store.add_cert("not a certificate"), std::runtime_error);
}

TEST_F(ClientCertStore, add_cert_stores_certificate)
{
    mp::ClientCertStore cert_store{cert_dir};
    EXPECT_NO_THROW(cert_store.add_cert(cert_data));

    const auto content = cert_store.PEM_cert_chain();
    EXPECT_THAT(content, StrEq(cert_data));
}

TEST_F(ClientCertStore, verifyCertEmptyAddsCertAndReturnsTrue)
{
    mp::ClientCertStore cert_store{cert_dir};

    ASSERT_TRUE(cert_store.PEM_cert_chain().empty());

    EXPECT_TRUE(cert_store.verify_cert(cert_data));

    const auto content = cert_store.PEM_cert_chain();

    EXPECT_EQ(content, cert_data);
}

TEST_F(ClientCertStore, verifyCertInStoreReturnsTrue)
{
    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    mp::ClientCertStore cert_store{cert_dir};

    ASSERT_FALSE(cert_store.PEM_cert_chain().empty());

    EXPECT_TRUE(cert_store.verify_cert(cert_data));
}

TEST_F(ClientCertStore, addCertAlreadyExistingDoesNotAddAgain)
{
    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    mp::ClientCertStore cert_store{cert_dir};

    ASSERT_FALSE(cert_store.PEM_cert_chain().empty());

    EXPECT_NO_THROW(cert_store.add_cert(cert_data));

    const auto content = cert_store.PEM_cert_chain();

    EXPECT_EQ(content, cert_data);
}

TEST_F(ClientCertStore, storeEmptyReturnsTrueWhenNoCerts)
{
    mp::ClientCertStore cert_store{cert_dir};

    EXPECT_TRUE(cert_store.is_store_empty());
}

TEST_F(ClientCertStore, storeEmptyReturnsFalseWhenCertExists)
{
    const QDir dir{cert_dir};
    const auto cert_path = dir.filePath("multipass_client_certs.pem");
    mpt::make_file_with_content(cert_path, cert_data);

    mp::ClientCertStore cert_store{cert_dir};

    EXPECT_FALSE(cert_store.is_store_empty());
}

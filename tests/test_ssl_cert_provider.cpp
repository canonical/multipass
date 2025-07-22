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
#include "mock_logger.h"
#include "mock_platform.h"
#include "mock_standard_paths.h"
#include "temp_dir.h"

#include <multipass/ssl_cert_provider.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

struct SSLCertProviderFixture : public testing::Test
{
    mpt::TempDir temp_dir;
    mp::Path cert_dir{temp_dir.path() + "/test-cert"};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

TEST_F(SSLCertProviderFixture, createsCertAndKey)
{
    mp::SSLCertProvider cert_provider{cert_dir};

    auto pem_cert = cert_provider.PEM_certificate();
    auto pem_key = cert_provider.PEM_signing_key();
    EXPECT_THAT(pem_cert, StrNe(""));
    EXPECT_THAT(pem_key, StrNe(""));
}

TEST_F(SSLCertProviderFixture, importsExistingCertAndKey)
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

    const QDir dir{cert_dir};
    const auto key_path = dir.filePath("multipass_cert_key.pem");
    const auto cert_path = dir.filePath("multipass_cert.pem");

    mpt::make_file_with_content(key_path, key_data);
    mpt::make_file_with_content(cert_path, cert_data);

    mp::SSLCertProvider cert_provider{cert_dir};

    EXPECT_THAT(cert_provider.PEM_signing_key(), StrEq(key_data));
    EXPECT_THAT(cert_provider.PEM_certificate(), StrEq(cert_data));
}

TEST_F(SSLCertProviderFixture, persistsCertAndKey)
{
    QDir dir{cert_dir};
    auto key_file = dir.filePath("multipass_cert_key.pem");
    auto cert_file = dir.filePath("multipass_cert.pem");

    EXPECT_FALSE(QFile::exists(key_file));
    EXPECT_FALSE(QFile::exists(cert_file));

    mp::SSLCertProvider cert_provider{cert_dir};

    EXPECT_TRUE(QFile::exists(key_file));
    EXPECT_TRUE(QFile::exists(cert_file));
}

TEST_F(SSLCertProviderFixture, createsDifferentCertsPerServerName)
{
    const auto& mock_paths = mpt::MockStandardPaths::mock_instance();
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(mock_paths, writableLocation(mp::StandardPaths::GenericDataLocation))
        .WillRepeatedly(Return(cert_dir));

    mp::SSLCertProvider cert_provider1{cert_dir, "test_server1"};
    mp::SSLCertProvider cert_provider2{cert_dir, "test_server2"};

    auto pem_cert1 = cert_provider1.PEM_certificate();
    auto pem_key1 = cert_provider1.PEM_signing_key();
    auto pem_cert2 = cert_provider2.PEM_certificate();
    auto pem_key2 = cert_provider2.PEM_signing_key();
    EXPECT_THAT(pem_cert1, StrNe(pem_cert2));
    EXPECT_THAT(pem_key1, StrNe(pem_key2));
}

TEST_F(SSLCertProviderFixture, reusesExistingValidServerCertificates)
{
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::GenericDataLocation))
        .WillRepeatedly(Return(cert_dir));

    constexpr auto root_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIBzjCCAXWgAwIBAgIUFSHy1TV98cz/ZOvfMBXOdgH02oYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMzUwNzA4MDg1OTM5WjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABIq/jXfQ+0U3
DYfNb54xG3EKiBC+SDluEOJyELQkg9kGXP2Yvh8d7tWN99bc63Bju1uAR4tWhGxP
EJt8PbSL88ajUzBRMB0GA1UdDgQWBBSOKSfZjt+7cfRspiOTU2I5a6NmVjAfBgNV
HSMEGDAWgBSOKSfZjt+7cfRspiOTU2I5a6NmVjAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0cAMEQCIChkSDoKa5iZqptHa9Ih7267WSYxx2h0nzOZxopZWUMx
AiAr+aaVzBBXe31uTuGvjiv/KccZHp1Rn/vaCOgbDxFATw==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIB2jCCAYCgAwIBAgIUCl9D+5RERQiuLKYhDXnTHb+z2QYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMjYwNzEwMDg1OTM5WjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATlZbmi2M8q9SR+Cgd6C/pA
AfuqGqznWizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
o2YwZDAUBgNVHREEDTALgglsb2NhbGhvc3QwHQYDVR0OBBYEFPAgeiZPxzoczlQ5
pJrcHJWugdvYMB8GA1UdIwQYMBaAFI4pJ9mO37tx9GymI5NTYjlro2ZWMAwGA1Ud
EwEB/wQCMAAwCgYIKoZIzj0EAwIDSAAwRQIgelfVfOSRmfsEMxxgWuZw6uMQCdFV
BZPeiPY0ZxjUPMcCIQChuXlX+ZuzLHPfv3KzCq11P3Y1dqNF4k7QQOl+Wrtl6w==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_key_contents =
        R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert";

    const QDir dir{cert_dir};
    const auto root_cert_path = QString::fromStdU16String(mp::get_root_cert_path().u16string());
    const auto cert_path = dir.filePath("localhost.pem");
    const auto key_path = dir.filePath("localhost_key.pem");

    mpt::make_file_with_content(root_cert_path, root_cert_contents);
    mpt::make_file_with_content(cert_path, subordinate_cert_contents);
    mpt::make_file_with_content(key_path, subordinate_key_contents);

    logger_scope.mock_logger->expect_log(mpl::Level::debug, "are valid X.509 files");
    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         "Re-using existing certificates for the gRPC server");

    mp::SSLCertProvider cert_provider{cert_dir, "localhost"};

    const auto actual_cert = cert_provider.PEM_certificate();
    const auto actual_key = cert_provider.PEM_signing_key();

    EXPECT_THAT(actual_cert, StrEq(subordinate_cert_contents));
    EXPECT_THAT(actual_key, StrEq(subordinate_key_contents));
}

TEST_F(SSLCertProviderFixture, regeneratesCertificatesWhenRootCertIsCorrupt)
{
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::GenericDataLocation))
        .WillRepeatedly(Return(cert_dir));

    constexpr auto root_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIBzjCCAXWgAwIBAgIUFSHy1TV98cz/ZOvfMBXOdgH02oYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMzUwNzA4MDg1OTM5WjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGBy49AgEGCCqGSM49AwEHA0IABIq/jXfQ+0U3
DYfNb54xG3EKiBC+SDluEOJyELQkg9kGXP2Yvh8d7tWN99bc63Bju1uAR4tWhGxP
EJt8PbSL88ajUzBRMB0GA1UdDgQWBBSOKSfZjt+7cfRspiOTU2I5a6NmVjAfBgNV
HSMEGDAWgBSOKSfZjt+7cfRspiOTU2I5a6NmVjAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0cAMEQCIChkSDoKa5iZqptHa9Ih7267WSYxx2h0nzOZxopZWUMx
AiAr+aaVzBBXe31uTuGvjiv/KccZHp1Rn/vaCOgbDxFATw==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIB2jCCAYCgAwIBAgIUCl9D+5RERQiuLKYhDXnTHb+z2QYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMjYwNzEwMDg1OTM5WjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATlZbmi2M8q9SR+Cgd6C/pA
AfuqGqznWizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
o2YwZDAUBgNVHREEDTALgglsb2NhbGhvc3QwHQYDVR0OBBYEFPAgeiZPxzoczlQ5
pJrcHJWugdvYMB8GA1UdIwQYMBaAFI4pJ9mO37tx9GymI5NTYjlro2ZWMAwGA1Ud
EwEB/wQCMAAwCgYIKoZIzj0EAwIDSAAwRQIgelfVfOSRmfsEMxxgWuZw6uMQCdFV
BZPeiPY0ZxjUPMcCIQChuXlX+ZuzLHPfv3KzCq11P3Y1dqNF4k7QQOl+Wrtl6w==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_key_contents =
        R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert";

    const QDir dir{cert_dir};
    const auto root_cert_path = QString::fromStdU16String(mp::get_root_cert_path().u16string());
    const auto cert_path = dir.filePath("localhost.pem");
    const auto key_path = dir.filePath("localhost_key.pem");

    mpt::make_file_with_content(root_cert_path, root_cert_contents);
    mpt::make_file_with_content(cert_path, subordinate_cert_contents);
    mpt::make_file_with_content(key_path, subordinate_key_contents);

    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Could not load either of");
    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         "Regenerating certificates for the gRPC server");

    mp::SSLCertProvider cert_provider{cert_dir, "localhost"};

    const auto actual_cert = cert_provider.PEM_certificate();
    const auto actual_key = cert_provider.PEM_signing_key();

    EXPECT_THAT(actual_cert, StrNe(subordinate_cert_contents));
    EXPECT_THAT(actual_key, StrNe(subordinate_key_contents));
}

TEST_F(SSLCertProviderFixture, regeneratesCertificatesWhenRootCertIsMissing)
{
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::GenericDataLocation))
        .WillRepeatedly(Return(cert_dir));

    constexpr auto subordinate_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIB2jCCAYCgAwIBAgIUCl9D+5RERQiuLKYhDXnTHb+z2QYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMjYwNzEwMDg1OTM5WjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATlZbmi2M8q9SR+Cgd6C/pA
AfuqGqznWizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
o2YwZDAUBgNVHREEDTALgglsb2NhbGhvc3QwHQYDVR0OBBYEFPAgeiZPxzoczlQ5
pJrcHJWugdvYMB8GA1UdIwQYMBaAFI4pJ9mO37tx9GymI5NTYjlro2ZWMAwGA1Ud
EwEB/wQCMAAwCgYIKoZIzj0EAwIDSAAwRQIgelfVfOSRmfsEMxxgWuZw6uMQCdFV
BZPeiPY0ZxjUPMcCIQChuXlX+ZuzLHPfv3KzCq11P3Y1dqNF4k7QQOl+Wrtl6w==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_key_contents =
        R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert";

    const QDir dir{cert_dir};
    const auto root_cert_path = QString::fromStdU16String(mp::get_root_cert_path().u16string());
    const auto cert_path = dir.filePath("localhost.pem");
    const auto key_path = dir.filePath("localhost_key.pem");

    mpt::make_file_with_content(cert_path, subordinate_cert_contents);
    mpt::make_file_with_content(key_path, subordinate_key_contents);

    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         "Regenerating certificates for the gRPC server");

    mp::SSLCertProvider cert_provider{cert_dir, "localhost"};

    const auto actual_cert = cert_provider.PEM_certificate();
    const auto actual_key = cert_provider.PEM_signing_key();

    EXPECT_THAT(actual_cert, StrNe(subordinate_cert_contents));
    EXPECT_THAT(actual_key, StrNe(subordinate_key_contents));
}

TEST_F(SSLCertProviderFixture, regeneratesCertificatesWhenSubordCertIsMissing)
{
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::GenericDataLocation))
        .WillRepeatedly(Return(cert_dir));

    constexpr auto root_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIBzjCCAXWgAwIBAgIUFSHy1TV98cz/ZOvfMBXOdgH02oYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMzUwNzA4MDg1OTM5WjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGBy49AgEGCCqGSM49AwEHA0IABIq/jXfQ+0U3
DYfNb54xG3EKiBC+SDluEOJyELQkg9kGXP2Yvh8d7tWN99bc63Bju1uAR4tWhGxP
EJt8PbSL88ajUzBRMB0GA1UdDgQWBBSOKSfZjt+7cfRspiOTU2I5a6NmVjAfBgNV
HSMEGDAWgBSOKSfZjt+7cfRspiOTU2I5a6NmVjAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0cAMEQCIChkSDoKa5iZqptHa9Ih7267WSYxx2h0nzOZxopZWUMx
AiAr+aaVzBBXe31uTuGvjiv/KccZHp1Rn/vaCOgbDxFATw==
-----END CERTIFICATE-----)cert";

    const QDir dir{cert_dir};
    const auto root_cert_path = QString::fromStdU16String(mp::get_root_cert_path().u16string());
    const auto cert_path = dir.filePath("localhost.pem");
    const auto key_path = dir.filePath("localhost_key.pem");

    mpt::make_file_with_content(root_cert_path, root_cert_contents);

    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         "Regenerating certificates for the gRPC server");

    mp::SSLCertProvider cert_provider{cert_dir, "localhost"};

    const auto actual_cert = cert_provider.PEM_certificate();
    const auto actual_key = cert_provider.PEM_signing_key();

    EXPECT_THAT(actual_cert, StrNe(""));
    EXPECT_THAT(actual_key, StrNe(""));
}

TEST_F(SSLCertProviderFixture, regeneratesCertificatesWhenSubordCertIsCorrupt)
{
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::GenericDataLocation))
        .WillRepeatedly(Return(cert_dir));

    constexpr auto root_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIBzjCCAXWgAwIBAgIUFSHy1TV98cz/ZOvfMBXOdgH02oYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMzUwNzA4MDg1OTM5WjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABIq/jXfQ+0U3
DYfNb54xG3EKiBC+SDluEOJyELQkg9kGXP2Yvh8d7tWN99bc63Bju1uAR4tWhGxP
EJt8PbSL88ajUzBRMB0GA1UdDgQWBBSOKSfZjt+7cfRspiOTU2I5a6NmVjAfBgNV
HSMEGDAWgBSOKSfZjt+7cfRspiOTU2I5a6NmVjAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0cAMEQCIChkSDoKa5iZqptHa9Ih7267WSYxx2h0nzOZxopZWUMx
AiAr+aaVzBBXe31uTuGvjiv/KccZHp1Rn/vaCOgbDxFATw==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIB2jCCAYCgAwIBAgIUCl9D+5RERQiuLKYhDXnTHb+z2QYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMjYwNzEwMDg1OTM5WjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATlZbmi2M8q9SR+Cgd6C/pA
AfuqGqznWizZyQgYv6Z/AosKpE6YXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
o2YwZDAUBgNVHREEDTALgglsb2NhbGhvc3QwHQYDVR0OBBYEFPAgeiZPxzoczlQ5
pJrcHJWugdvYMB8GA1UdIwQYMBaAFI4pJ9mO37tx9GymI5NTYjlro2ZWMAwGA1Ud
EwEB/wQCMAAwCgYIKoZIzj0EAwIDSAAwRQIgelfVfOSRmfsEMxxgWuZw6uMQCdFV
BZPeiPY0ZxjUPMcCIQChuXlX+ZuzLHPfv3KzCq11P3Y1dqNF4k7QQOl+Wrtl6w==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_key_contents =
        R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert";

    const QDir dir{cert_dir};
    const auto root_cert_path = QString::fromStdU16String(mp::get_root_cert_path().u16string());
    const auto cert_path = dir.filePath("localhost.pem");
    const auto key_path = dir.filePath("localhost_key.pem");

    mpt::make_file_with_content(root_cert_path, root_cert_contents);
    mpt::make_file_with_content(cert_path, subordinate_cert_contents);
    mpt::make_file_with_content(key_path, subordinate_key_contents);

    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Could not load either of");
    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         "Regenerating certificates for the gRPC server");

    mp::SSLCertProvider cert_provider{cert_dir, "localhost"};

    const auto actual_cert = cert_provider.PEM_certificate();
    const auto actual_key = cert_provider.PEM_signing_key();

    EXPECT_THAT(actual_cert, StrNe(subordinate_cert_contents));
    EXPECT_THAT(actual_key, StrNe(subordinate_key_contents));
}

TEST_F(SSLCertProviderFixture, regeneratesCertificatesWhenRootCertIsNotTheIssuer)
{
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::GenericDataLocation))
        .WillRepeatedly(Return(cert_dir));

    constexpr auto root_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIBzzCCAXWgAwIBAgIUAOtHJnyORHMBZb1g3Lub65TFMkgwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMTAzODU2WhcNMzUwNzA4MTAzODU2WjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABOZrJ6hk0ARU
fYynzij63WUdFGXASPzrshLxxzum2FfADoCHfj28O6OiVFTBF5T0q39oHGaxclgV
99fJFjgw0GCjUzBRMB0GA1UdDgQWBBSJAiHfyxwx7DC5ru7QGjFr35H0xTAfBgNV
HSMEGDAWgBSJAiHfyxwx7DC5ru7QGjFr35H0xTAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0gAMEUCICVVwSYOZqyPmd1aXkVCDQHMFm6hOM7hFs/6SRBQqAWZ
AiEAmhbAFiEUSyjZj5MVOhw1TW6NxGe2+45ypLULJaOAE0g=
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_cert_contents =
        R"cert(-----BEGIN CERTIFICATE-----
MIIB2jCCAYCgAwIBAgIUCl9D+5RERQiuLKYhDXnTHb+z2QYwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUwNzEwMDg1OTM5WhcNMjYwNzEwMDg1OTM5WjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAATlZbmi2M8q9SR+Cgd6C/pA
AfuqGqznWizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
o2YwZDAUBgNVHREEDTALgglsb2NhbGhvc3QwHQYDVR0OBBYEFPAgeiZPxzoczlQ5
pJrcHJWugdvYMB8GA1UdIwQYMBaAFI4pJ9mO37tx9GymI5NTYjlro2ZWMAwGA1Ud
EwEB/wQCMAAwCgYIKoZIzj0EAwIDSAAwRQIgelfVfOSRmfsEMxxgWuZw6uMQCdFV
BZPeiPY0ZxjUPMcCIQChuXlX+ZuzLHPfv3KzCq11P3Y1dqNF4k7QQOl+Wrtl6w==
-----END CERTIFICATE-----)cert";

    constexpr auto subordinate_key_contents =
        R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert";

    const QDir dir{cert_dir};
    const auto root_cert_path = QString::fromStdU16String(mp::get_root_cert_path().u16string());
    const auto cert_path = dir.filePath("localhost.pem");
    const auto key_path = dir.filePath("localhost_key.pem");

    mpt::make_file_with_content(root_cert_path, root_cert_contents);
    mpt::make_file_with_content(cert_path, subordinate_cert_contents);
    mpt::make_file_with_content(key_path, subordinate_key_contents);

    logger_scope.mock_logger->expect_log(mpl::Level::debug, "are valid X.509 files");
    logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                         "is not the signer of the gRPC server certificate");
    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         "Regenerating certificates for the gRPC server");

    mp::SSLCertProvider cert_provider{cert_dir, "localhost"};

    const auto actual_cert = cert_provider.PEM_certificate();
    const auto actual_key = cert_provider.PEM_signing_key();

    EXPECT_THAT(actual_cert, StrNe(subordinate_cert_contents));
    EXPECT_THAT(actual_key, StrNe(subordinate_key_contents));
}

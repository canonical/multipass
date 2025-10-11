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
    const auto [mock_platform, _] = mpt::MockPlatform::inject<NiceMock>();
    // move the multipass_root_cert.pem into the temporary directory so it will be deleted
    // automatically later
    EXPECT_CALL(*mock_platform, get_root_cert_dir())
        .WillRepeatedly(Return(std::filesystem::path{cert_dir.toStdU16String()}));

    mp::SSLCertProvider cert_provider1{cert_dir, "test_server1"};
    mp::SSLCertProvider cert_provider2{cert_dir, "test_server2"};

    auto pem_cert1 = cert_provider1.PEM_certificate();
    auto pem_key1 = cert_provider1.PEM_signing_key();
    auto pem_cert2 = cert_provider2.PEM_certificate();
    auto pem_key2 = cert_provider2.PEM_signing_key();
    EXPECT_THAT(pem_cert1, StrNe(pem_cert2));
    EXPECT_THAT(pem_key1, StrNe(pem_key2));
}

struct SSLCertTestParam
{
    std::string root_cert;
    std::string subordinate_cert;
    std::string subordinate_key;
    std::vector<std::pair<mpl::Level, std::string>> expected_logs;
    bool regenerate = true;
};

class SSLCertProviderParameterTests : public SSLCertProviderFixture,
                                      public testing::WithParamInterface<SSLCertTestParam>
{
};

TEST_P(SSLCertProviderParameterTests, regeneratesCertificates)
{
    const auto& param = GetParam();

    const auto [mock_platform, _] = mpt::MockPlatform::inject<NiceMock>();
    EXPECT_CALL(*mock_platform, get_root_cert_dir())
        .WillRepeatedly(Return(std::filesystem::path{cert_dir.toStdU16String()}));

    const QDir dir{cert_dir};
    const auto root_cert_path =
        QString::fromStdU16String(MP_PLATFORM.get_root_cert_path().u16string());
    const auto cert_path = dir.filePath("localhost.pem");
    const auto key_path = dir.filePath("localhost_key.pem");

    if (!param.root_cert.empty())
        mpt::make_file_with_content(root_cert_path, param.root_cert);
    if (!param.subordinate_cert.empty())
        mpt::make_file_with_content(cert_path, param.subordinate_cert);
    if (!param.subordinate_key.empty())
        mpt::make_file_with_content(key_path, param.subordinate_key);

    // Set up expected log messages
    for (const auto& [level, message] : param.expected_logs)
    {
        logger_scope.mock_logger->expect_log(level, message);
    }

    mp::SSLCertProvider cert_provider{cert_dir, "localhost"};

    const auto actual_cert = cert_provider.PEM_certificate();
    const auto actual_key = cert_provider.PEM_signing_key();

    if (param.regenerate)
    {
        EXPECT_THAT(actual_cert, StrNe(param.subordinate_cert));
        EXPECT_THAT(actual_key, StrNe(param.subordinate_key));
    }
    else
    {
        EXPECT_THAT(actual_cert, StrEq(param.subordinate_cert));
        EXPECT_THAT(actual_key, StrEq(param.subordinate_key));
    }
}

INSTANTIATE_TEST_SUITE_P(
    SSLCertProviderTests,
    SSLCertProviderParameterTests,
    testing::Values(
        SSLCertTestParam{// Valid server certificates
                         R"cert(-----BEGIN CERTIFICATE-----
MIIBzjCCAXWgAwIBAgIUUxRU151mY5cjo8V62XTPsSArbqowCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUxMDExMDQ0ODQzWhcNMzUxMDA5MDQ0ODQzWjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABGxi3pMImRW+
PU5VmyLrxPXdzyyKYg2Rxh283M0br2LZnit0E6mBc7TTNaGmeA2UMrnWXAHQ0Al6
80IxDo1MuJujUzBRMB0GA1UdDgQWBBTxx5ieDIt2XyjwnEplC9UmvFu5qTAfBgNV
HSMEGDAWgBTxx5ieDIt2XyjwnEplC9UmvFu5qTAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0cAMEQCIEFeoff2SpUQMmpknLuRdkVv2V22GeYyRuzhST8bRBm/
AiBlHZrSslurf3k2upXxypUktY8gO9fmhFBeZHIOkVbOzA==
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN CERTIFICATE-----
MIIB9TCCAZygAwIBAgIUVl/7R6Ps3Lpf5OT1bI/zBSu0ItswCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUxMDExMDQ0ODQzWhcNMjYxMDExMDQ0ODQzWjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASf4wQpBqlhk+as4uXUCVVm
2q2zlG0wb7B6QSM+V5IXPA5h2OlI3dIz7dQtiAr1CbQLTSuCN+WhPPmRDDFWWBUB
o4GBMH8wDAYDVR0TAQH/BAIwADAaBgNVHREEEzARgglsb2NhbGhvc3SHBH8AAAEw
EwYDVR0lBAwwCgYIKwYBBQUHAwEwHQYDVR0OBBYEFGEWVChbpWLb4bY5Vd1rM9GR
7ebtMB8GA1UdIwQYMBaAFPHHmJ4Mi3ZfKPCcSmUL1Sa8W7mpMAoGCCqGSM49BAMC
A0cAMEQCIFA8fMlJqtheFmdIhNUiRmlGH/Xd6WJGa+nj3vm4HtPmAiBvVlI5GaOg
y5aCa2JgVvkIVcRPDBCQwoaFD9Y4Kxxoig==
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgp0z/cELvz+iLPyoD
PCfTSsi5B+QwbetVXso0IycHCK6hRANCAASf4wQpBqlhk+as4uXUCVVm2q2zlG0w
b7B6QSM+V5IXPA5h2OlI3dIz7dQtiAr1CbQLTSuCN+WhPPmRDDFWWBUB
-----END PRIVATE KEY-----)cert",
                         {{mpl::Level::debug, "are valid X.509 files"},
                          {mpl::Level::info, "Re-using existing certificates for the gRPC server"}},
                         false},
        SSLCertTestParam{// Missing root cert
                         "",
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
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert",
                         {{mpl::Level::info, "Regenerating certificates for the gRPC server"}}},
        SSLCertTestParam{// Missing subordinate cert
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
-----END CERTIFICATE-----)cert",
                         "",
                         "",
                         {{mpl::Level::info, "Regenerating certificates for the gRPC server"}}},
        SSLCertTestParam{// Corrupt root cert
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
-----END CERTIFICATE-----)cert",
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
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert",
                         {{mpl::Level::warning, "Could not load either of"},
                          {mpl::Level::info, "Regenerating certificates for the gRPC server"}}},
        SSLCertTestParam{// Corrupt subordinate cert
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
-----END CERTIFICATE-----)cert",
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
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGNUltvugUGTeKVu1
0txykTDHfS2nlRGuRUCEHw5KKJuhRANCAATlZbmi2M8q9SR+Cgd6C/pAAfuqGqzn
WizZyQgYv6Z/AosKpE6DcyIYXGuGn2U/Icpxsn/ZycRel2shM4dP5OBg
-----END PRIVATE KEY-----)cert",
                         {{mpl::Level::warning, "Could not load either of"},
                          {mpl::Level::info, "Regenerating certificates for the gRPC server"}}},
        SSLCertTestParam{// Root cert not the signer of server cert
                         R"cert(-----BEGIN CERTIFICATE-----
MIIB0TCCAXegAwIBAgIUYJpTTkCvlm6CU8Ufy3bD01+uxnwwCgYIKoZIzj0EAwIw
PjELMAkGA1UEBhMCVVMxEzARBgNVBAoMCnNoYXJkZXI5OTYxGjAYBgNVBAMMEU11
bHRpcGFzcyBSb290IENBMB4XDTI1MTAxMTA0NTE1MloXDTM1MTAwOTA0NTE1Mlow
PjELMAkGA1UEBhMCVVMxEzARBgNVBAoMCnNoYXJkZXI5OTYxGjAYBgNVBAMMEU11
bHRpcGFzcyBSb290IENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEX5HsyHve
0u6jWMmZAmdZ1Muh5HQibKl4xMOojk0t81EQRE2oFOxUceFF0LeRisYaV1H/n9+3
oJNNUKNknSYCMqNTMFEwHQYDVR0OBBYEFKx9h895VsGvkmouH6o0tWDzW/RxMB8G
A1UdIwQYMBaAFKx9h895VsGvkmouH6o0tWDzW/RxMA8GA1UdEwEB/wQFMAMBAf8w
CgYIKoZIzj0EAwIDSAAwRQIgCTY7PXLmD0zwV3IBOexi71gf6ZGSlItjXks1bRG8
YiACIQDDe4jtIZTP7Kw06nr61PejvfWDnQhDskfqOkHpKpTvPA==
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN CERTIFICATE-----
MIIB9jCCAZygAwIBAgIUVl/7R6Ps3Lpf5OT1bI/zBSu0ItwwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUxMDExMDQ1MTUyWhcNMjYxMDExMDQ1MTUyWjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASm04ivEWfHpm28SJ2aVfac
tw+0p35vItYs3p91rRPh5Etm4n/NTQbRBYTPXm08XwI+bmtVetrTCW5TfZ/USAhy
o4GBMH8wDAYDVR0TAQH/BAIwADAaBgNVHREEEzARgglsb2NhbGhvc3SHBH8AAAEw
EwYDVR0lBAwwCgYIKwYBBQUHAwEwHQYDVR0OBBYEFG4V9sM3BfD6VDz8LpCqkgC9
ZMX8MB8GA1UdIwQYMBaAFMCy5nYv8B0fquwB2yhc0+tC0xOqMAoGCCqGSM49BAMC
A0gAMEUCICJuye/aUH56WDt9f2iuFMyz7SdKLHFyYWUf81NPv7k5AiEA3MECztyW
REjMJHprgc63THRbmW8S4ksD6q0x3pa4iGo=
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgoAanPdCFx78AskWe
eUL60fl0qAuzjd1vrAR1Z2yEEA+hRANCAASm04ivEWfHpm28SJ2aVfactw+0p35v
ItYs3p91rRPh5Etm4n/NTQbRBYTPXm08XwI+bmtVetrTCW5TfZ/USAhy
-----END PRIVATE KEY-----)cert",
                         {{mpl::Level::debug, "are valid X.509 files"},
                          {mpl::Level::warning, "is not the signer of the gRPC server certificate"},
                          {mpl::Level::info, "Regenerating certificates for the gRPC server"}}},
        SSLCertTestParam{// Expired server cert
                         R"cert(-----BEGIN CERTIFICATE-----
MIIBzzCCAXWgAwIBAgIUTwg6yEhLKbaNOkPKptZjdA7nPfEwCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUxMDExMDQ1MzEzWhcNMzUxMDA5MDQ1MzEzWjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABPN5wC4zJPtq
Rvob6784LKGPcm3nQvkNGwPnDiKEQW2+9XjJZyajSRMXqGi9HApjKrdtLnpahpV3
nozuFn4MhHyjUzBRMB0GA1UdDgQWBBRgm8a0EFMw7o4vSO5TBvVJX3wQ3zAfBgNV
HSMEGDAWgBRgm8a0EFMw7o4vSO5TBvVJX3wQ3zAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0gAMEUCIFFxpHWbhmADPRqlJzd3UW8UuJ31X+svjvRNp96BV3As
AiEAv5TNEYgDkbFZL+mdxkRpMcIqMKTeQ5XJAAUNSccpuMU=
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN CERTIFICATE-----
MIIB9jCCAZygAwIBAgIUVl/7R6Ps3Lpf5OT1bI/zBSu0It0wCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUxMDExMDQ1MzEzWhcNMjUxMDExMDQ1MzEzWjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQ+RWFT7EsmiVjsmuNVHLST
VJts+tbI4TUAI+Glb6vEsgNaxcb5OEcWenpZ4lkVCVRM2+vUJEb0Ma595UtSp3y9
o4GBMH8wDAYDVR0TAQH/BAIwADAaBgNVHREEEzARgglsb2NhbGhvc3SHBH8AAAEw
EwYDVR0lBAwwCgYIKwYBBQUHAwEwHQYDVR0OBBYEFD/ISUn5L/pYpsYChcGAdMEq
KXAxMB8GA1UdIwQYMBaAFGCbxrQQUzDuji9I7lMG9UlffBDfMAoGCCqGSM49BAMC
A0gAMEUCIQCEILD1lEMj8yyISgb1XCW7Jmj4GqjJAzOpw65tKYQ1HwIgWE0g+/vQ
jJqg0xUWTVsNm6oyxqK+XL8/LaBYBMScrdY=
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgNZ1gdGGXXHECUjXV
BmHhpWWYsdTK3cYOaqR0HDU9NuehRANCAAQ+RWFT7EsmiVjsmuNVHLSTVJts+tbI
4TUAI+Glb6vEsgNaxcb5OEcWenpZ4lkVCVRM2+vUJEb0Ma595UtSp3y9
-----END PRIVATE KEY-----)cert",
                         {{mpl::Level::debug, "are valid X.509 files"},
                          {mpl::Level::warning, "validity period is not valid"},
                          {mpl::Level::info, "Regenerating certificates for the gRPC server"}}},
        SSLCertTestParam{// Server cert missing serverAuth extension
                         R"cert(-----BEGIN CERTIFICATE-----
MIIBzjCCAXWgAwIBAgIUO95PN/WButxgebA76W7ma7d+bP4wCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUxMDExMDQyMTQ3WhcNMzUxMDA5MDQyMTQ3WjA9
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0
aXBhc3MgUm9vdCBDQTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABGNRoMsA+XTv
syS14ycFub9OJCfKIleVPXL/g8yy5ttDQ9ARwhRplGVboheXvW4ix5bRfbi7TI/3
R8Q7BMzOzz+jUzBRMB0GA1UdDgQWBBS8Au0BSmlbWvBJZc9P0Z9YAji/ajAfBgNV
HSMEGDAWgBS8Au0BSmlbWvBJZc9P0Z9YAji/ajAPBgNVHRMBAf8EBTADAQH/MAoG
CCqGSM49BAMCA0cAMEQCIEyrRmyakaFNfsv7y93WSFu3kQiSvwhPQGyU5/rmvgAq
AiA0b0p2vhWbgQ36xci+OAUimRERZc7xm6Kq/BstsohmBw==
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN CERTIFICATE-----
MIIB4TCCAYagAwIBAgIUVl/7R6Ps3Lpf5OT1bI/zBSu0ItowCgYIKoZIzj0EAwIw
PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs
dGlwYXNzIFJvb3QgQ0EwHhcNMjUxMDExMDQyMTQ3WhcNMjYxMDExMDQyMTQ3WjA1
MQswCQYDVQQGEwJVUzESMBAGA1UECgwJQ2Fub25pY2FsMRIwEAYDVQQDDAlsb2Nh
bGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQXxQohKM5ZXFPhydtPxvkv
lfycEVRQUwtxGnpEhj0D0h1M3tKVz3HkLh/yCS2b2jtZJRq3lzbfwHkOkFk64iqs
o2wwajAMBgNVHRMBAf8EAjAAMBoGA1UdEQQTMBGCCWxvY2FsaG9zdIcEfwAAATAd
BgNVHQ4EFgQU+kIJ1nqffb6P4lSN9XBkqgIOmQYwHwYDVR0jBBgwFoAUvALtAUpp
W1rwSWXPT9GfWAI4v2owCgYIKoZIzj0EAwIDSQAwRgIhAL5llYus/xGn7f5ibsmG
vwYu01mkSZKHpMOCGCLYSqV8AiEAmDfgqgilMt2FkJ2LJLe+nTOShvqG6VWHQOxC
uJiZkGQ=
-----END CERTIFICATE-----)cert",
                         R"cert(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgWD5l1u681lEtS2s4
lxScmmgQpHiVy2dRvF3Qy5UCwRihRANCAAQXxQohKM5ZXFPhydtPxvkvlfycEVRQ
UwtxGnpEhj0D0h1M3tKVz3HkLh/yCS2b2jtZJRq3lzbfwHkOkFk64iqs
-----END PRIVATE KEY-----)cert",
                         {{mpl::Level::debug, "are valid X.509 files"},
                          {mpl::Level::warning, "does not contain the correct extensions"},
                          {mpl::Level::info, "Regenerating certificates for the gRPC server"}}}));

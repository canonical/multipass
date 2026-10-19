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

#pragma once

#include <multipass/cert_provider.h>

using namespace testing;

namespace multipass::test
{
// Generated using OpenSSL command:
// # 1. Generate root CA private key
// openssl ecparam -name prime256v1 -genkey -noout -out root_key.pem
// # 2. Generate root CA certificate (self-signed)
// openssl req -new -x509 -key root_key.pem -out root_cert.pem -days 2600000
//     -subj "/C=US/O=Canonical/CN=Multipass Root CA"
//     -addext "basicConstraints=critical,CA:TRUE"
// # 3. Generate server/client private key
// openssl ecparam -name prime256v1 -genkey -noout -out cert_key.pem
// # 4. Generate certificate signing request
// openssl req -new -key cert_key.pem -out cert.csr
//     -subj "/C=US/O=Canonical/CN=localhost"
// # 5. Create extension file for SAN
// echo "subjectAltName=DNS:localhost" > cert_ext.cnf
// # 6. Sign certificate with root CA
// openssl x509 -req -in cert.csr -CA root_cert.pem -CAkey root_key.pem
//     -CAcreateserial -out cert.pem -days 2600000
//     -extfile cert_ext.cnf
// # 7. Verify the certificate chain
// openssl verify -CAfile root_cert.pem cert.pem
constexpr auto root_cert = "-----BEGIN CERTIFICATE-----\n"
                           "MIIB0DCCAXegAwIBAgIUHnKVDJqpyPbwk4n/6S8MrTJqFlUwCgYIKoZIzj0EAwIw\n"
                           "PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs\n"
                           "dGlwYXNzIFJvb3QgQ0EwIBcNMjYwMTI5MjEyMzA3WhgPOTE0NDA4MjEyMTIzMDda\n"
                           "MD0xCzAJBgNVBAYTAlVTMRIwEAYDVQQKDAlDYW5vbmljYWwxGjAYBgNVBAMMEU11\n"
                           "bHRpcGFzcyBSb290IENBMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEbhgEXIz/\n"
                           "V0NKrVeXmybqq2mszTI/dz1OTNrn4BxCruefje/T/7xV3gcpUB57wMn3odfSQ882\n"
                           "1FNuLBNb+QHSY6NTMFEwHQYDVR0OBBYEFNsPrEFncOxkBGClSh+y1/Ml+WF/MB8G\n"
                           "A1UdIwQYMBaAFNsPrEFncOxkBGClSh+y1/Ml+WF/MA8GA1UdEwEB/wQFMAMBAf8w\n"
                           "CgYIKoZIzj0EAwIDRwAwRAIgfyC10LX0dMLfHX/VY0WcrFy89LI0Q8yfy+xe6p75\n"
                           "8jICIEaa50N5pAD19HPjaleD3gsKP2XBKnBCXYqtTxYEyjZE\n"
                           "-----END CERTIFICATE-----\n";

// cert and key are used as both server certificate and client certificate in the unit test
// environment
constexpr auto cert = "-----BEGIN CERTIFICATE-----\n"
                      "MIIBzzCCAXSgAwIBAgIUeYLD6av03lco3NgJsV1UikBL8fYwCgYIKoZIzj0EAwIw\n"
                      "PTELMAkGA1UEBhMCVVMxEjAQBgNVBAoMCUNhbm9uaWNhbDEaMBgGA1UEAwwRTXVs\n"
                      "dGlwYXNzIFJvb3QgQ0EwIBcNMjYwMTI5MjEyMzMwWhgPOTE0NDA4MjEyMTIzMzBa\n"
                      "MDUxCzAJBgNVBAYTAlVTMRIwEAYDVQQKDAlDYW5vbmljYWwxEjAQBgNVBAMMCWxv\n"
                      "Y2FsaG9zdDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABE3A3YcGBN97GoOs0iLm\n"
                      "lfMZBXa587RediiLDdV0fJ1dXWPlLDGWULX+QpxYJsA4I+axOse2SPDJG9e+RwFw\n"
                      "3nyjWDBWMBQGA1UdEQQNMAuCCWxvY2FsaG9zdDAdBgNVHQ4EFgQUyyqh82a3wUU5\n"
                      "z5S6TvhCkwi2BQswHwYDVR0jBBgwFoAU2w+sQWdw7GQEYKVKH7LX8yX5YX8wCgYI\n"
                      "KoZIzj0EAwIDSQAwRgIhAMzv/88WnDk++0VoBVb9IOmHkLYUaAzN+7zOzsdvCyNe\n"
                      "AiEA39vFGT9zq2EN04VsIpHWqSNOPnHww0RnYefRoqWsHi0=\n"
                      "-----END CERTIFICATE-----\n";

constexpr auto key = "-----BEGIN EC PRIVATE KEY-----\n"
                     "MHcCAQEEII6dtMwSlwyvXeWlpctH6OjPs+BQXvVdvtnmf/qp0EnhoAoGCCqGSM49\n"
                     "AwEHoUQDQgAETcDdhwYE33sag6zSIuaV8xkFdrnztF52KIsN1XR8nV1dY+UsMZZQ\n"
                     "tf5CnFgmwDgj5rE6x7ZI8Mkb175HAXDefA==\n"
                     "-----END EC PRIVATE KEY-----\n";

struct MockCertProvider : public CertProvider
{
    MockCertProvider()
    {
        ON_CALL(*this, PEM_certificate).WillByDefault(Return(cert));
        ON_CALL(*this, PEM_signing_key).WillByDefault(Return(key));
    }

    MOCK_METHOD(std::string, PEM_certificate, (), (override, const));
    MOCK_METHOD(std::string, PEM_signing_key, (), (override, const));
};
} // namespace multipass::test

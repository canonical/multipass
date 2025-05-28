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
constexpr auto root_cert = "-----BEGIN CERTIFICATE-----\n"
                           "MIIBvjCCAWWgAwIBAgIEUlzMbjAKBggqhkjOPQQDAjA9MQswCQYDVQQGEwJVUzES\n"
                           "MBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0aXBhc3MgUm9vdCBDQTAe\n"
                           "Fw0yNTAxMjkxMzAzNDBaFw0zNTAxMjcxMzAzNDBaMD0xCzAJBgNVBAYTAlVTMRIw\n"
                           "EAYDVQQKDAlDYW5vbmljYWwxGjAYBgNVBAMMEU11bHRpcGFzcyBSb290IENBMFkw\n"
                           "EwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAExnNTmDW0EMAg/ADMNJIc5V4BP/si6MlA\n"
                           "+YSHDTy+nbgXJ9q0/ORKZETjydqIkVRLzu1LR5sWpdbSzSPo3ft/9KNTMFEwHQYD\n"
                           "VR0OBBYEFAYlQy+QMhSOh/gACsgSdWbIH5NoMB8GA1UdIwQYMBaAFAYlQy+QMhSO\n"
                           "h/gACsgSdWbIH5NoMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwIDRwAwRAIg\n"
                           "ErA3KYoWLTkQ9J6cu/bSS539veIBO7p0xvb2x0u2UA0CIEiJ0mMdATr/I8tovsZm\n"
                           "xgvZMY2ColjLunUiNG8H096n\n"
                           "-----END CERTIFICATE-----\n";

// cert and key are used as both server certificate and client certificate in the unit test
// environment
constexpr auto cert = "-----BEGIN CERTIFICATE-----\n"
                      "MIIByjCCAXCgAwIBAgIENvdePTAKBggqhkjOPQQDAjA9MQswCQYDVQQGEwJVUzES\n"
                      "MBAGA1UECgwJQ2Fub25pY2FsMRowGAYDVQQDDBFNdWx0aXBhc3MgUm9vdCBDQTAe\n"
                      "Fw0yNTAxMjkxMzAzNDBaFw0yNjAxMjkxMzAzNDBaMDUxCzAJBgNVBAYTAlVTMRIw\n"
                      "EAYDVQQKDAlDYW5vbmljYWwxEjAQBgNVBAMMCWxvY2FsaG9zdDBZMBMGByqGSM49\n"
                      "AgEGCCqGSM49AwEHA0IABGAw4mRhGqCg7uSIsVgBIzMOoGnlEFWga2dxUzA1YwNe\n"
                      "8SB679smyb7KVsPg4fK/P7XS4ORxSnMVnKWvTAfYKXWjZjBkMBQGA1UdEQQNMAuC\n"
                      "CWxvY2FsaG9zdDAdBgNVHQ4EFgQU++FdgRpFokGT+7Fdgqe4SxmSD9UwHwYDVR0j\n"
                      "BBgwFoAUBiVDL5AyFI6H+AAKyBJ1Zsgfk2gwDAYDVR0TAQH/BAIwADAKBggqhkjO\n"
                      "PQQDAgNIADBFAiAesF7z8ItZVxK6fgUwhWfgN5rUFzCO5tBGJFDHU7eIZgIhALdl\n"
                      "2mAn2oocQZfHohrbVUIuWDiUr0SxOkdGUISX0ElJ\n"
                      "-----END CERTIFICATE-----\n";

constexpr auto key = "-----BEGIN PRIVATE KEY-----\n"
                     "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgwRNA3VMqakM32i0C\n"
                     "PHE5i4qRNGdvgXtCWwpp0gXv+oGhRANCAARgMOJkYRqgoO7kiLFYASMzDqBp5RBV\n"
                     "oGtncVMwNWMDXvEgeu/bJsm+ylbD4OHyvz+10uDkcUpzFZylr0wH2Cl1\n"
                     "-----END PRIVATE KEY-----\n";

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

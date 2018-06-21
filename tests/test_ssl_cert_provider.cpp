/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "multipass/ssl_cert_provider.h"

#include <QTemporaryDir>
#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

namespace
{
template <typename T>
void write(const QDir& dir, const char* name, const T& t)
{
    QFile output{dir.filePath(name)};
    auto opened = output.open(QIODevice::WriteOnly);
    if (!opened)
        throw std::runtime_error("test unable to open file for writing");

    auto written = output.write(t);
    if (written == -1)
        throw std::runtime_error("test unable to write data");

    output.close();
}
} // namespace
struct SSLCertProvider : public testing::Test
{
    SSLCertProvider()
    {
        if (!cert_dir.isValid())
            throw std::runtime_error("test failed to create temp directory");
    }
    QTemporaryDir cert_dir;
};

TEST_F(SSLCertProvider, creates_cert_and_key)
{
    mp::SSLCertProvider cert_provider{cert_dir.path()};

    auto pem_cert = cert_provider.PEM_certificate();
    auto pem_key = cert_provider.PEM_signing_key();
    EXPECT_THAT(pem_cert, StrNe(""));
    EXPECT_THAT(pem_key, StrNe(""));
}

TEST_F(SSLCertProvider, imports_existing_cert_and_key)
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

    constexpr auto cert_dir_name = "certificate";
    QDir dir{cert_dir.path()};
    auto made_path = dir.mkpath(cert_dir_name);
    if (!made_path)
        throw std::runtime_error("test failed to create temporary certificate directory");

    dir.cd(cert_dir_name);

    write(dir, "multipass_cert_key.pem", key_data);
    write(dir, "multipass_cert.pem", cert_data);

    mp::SSLCertProvider cert_provider{cert_dir.path()};

    EXPECT_THAT(cert_provider.PEM_signing_key(), StrEq(key_data));
    EXPECT_THAT(cert_provider.PEM_certificate(), StrEq(cert_data));
}

TEST_F(SSLCertProvider, persists_cert_and_key)
{
    constexpr auto cert_dir_name = "certificate";
    QDir dir{cert_dir.path()};
    auto made_path = dir.mkpath(cert_dir_name);
    if (!made_path)
        throw std::runtime_error("test failed to create temporary certificate directory");
    dir.cd(cert_dir_name);

    auto key_file = dir.filePath("multipass_cert_key.pem");
    auto cert_file = dir.filePath("multipass_cert.pem");

    EXPECT_FALSE(QFile::exists(key_file));
    EXPECT_FALSE(QFile::exists(cert_file));

    mp::SSLCertProvider cert_provider{cert_dir.path()};

    EXPECT_TRUE(QFile::exists(key_file));
    EXPECT_TRUE(QFile::exists(cert_file));
}

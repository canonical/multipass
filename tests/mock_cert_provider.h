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

#ifndef MULTIPASS_MOCK_CERT_PROVIDER_H
#define MULTIPASS_MOCK_CERT_PROVIDER_H

#include <multipass/cert_provider.h>

using namespace testing;

namespace multipass::test
{
constexpr auto client_cert = "-----BEGIN CERTIFICATE-----\n"
                             "MIIBUjCB+AIBKjAKBggqhkjOPQQDAjA1MQswCQYDVQQGEwJDQTESMBAGA1UECgwJ\n"
                             "Q2Fub25pY2FsMRIwEAYDVQQDDAlsb2NhbGhvc3QwHhcNMTgwNjIxMTM0MjI5WhcN\n"
                             "MTkwNjIxMTM0MjI5WjA1MQswCQYDVQQGEwJDQTESMBAGA1UECgwJQ2Fub25pY2Fs\n"
                             "MRIwEAYDVQQDDAlsb2NhbGhvc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQA\n"
                             "FGNAqq7c5IMDeQ/cV4+EmogmkfpbTLSPfXgXVLHRsvL04xUAkqGpL+eyGFVE6dqa\n"
                             "J7sAPJJwlVj1xD0r5DX5MAoGCCqGSM49BAMCA0kAMEYCIQCvI0PYv9f201fbe4LP\n"
                             "BowTeYWSqMQtLNjvZgd++AAGhgIhALNPW+NRSKCXwadiIFgpbjPInLPqXPskLWSc\n"
                             "aXByaQyt\n"
                             "-----END CERTIFICATE-----\n";
constexpr auto client_key = "-----BEGIN PRIVATE KEY-----\n"
                            "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgsSAz5ggzrLjai0I/\n"
                            "F0hYg5oG/shpXJiBQtJdBCG3lUShRANCAAQAFGNAqq7c5IMDeQ/cV4+Emogmkfpb\n"
                            "TLSPfXgXVLHRsvL04xUAkqGpL+eyGFVE6dqaJ7sAPJJwlVj1xD0r5DX5\n"
                            "-----END PRIVATE KEY-----\n";

struct MockCertProvider : public CertProvider
{
    MockCertProvider()
    {
        EXPECT_CALL(*this, PEM_certificate()).WillRepeatedly(Return(client_cert));
        EXPECT_CALL(*this, PEM_signing_key()).WillRepeatedly(Return(client_key));
    }

    MOCK_CONST_METHOD0(PEM_certificate, std::string());
    MOCK_CONST_METHOD0(PEM_signing_key, std::string());
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_CERT_PROVIDER_H

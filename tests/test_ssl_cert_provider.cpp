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

#include "src/cert/ssl_cert_provider.h"

#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

TEST(SSLCertProvider, creates_cert)
{
    mp::SSLCertProvider cert_provider;

    auto pem_cert = cert_provider.certificate_as_base64();
    EXPECT_THAT(pem_cert, StrNe(""));
}

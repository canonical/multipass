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

#include "common.h"
#include "mock_openssl_syscalls.h"

#include <multipass/passphrase_handler.h>

#include <multipass/format.h>
namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

TEST(PassphraseHandler, expectedHashReturned)
{
    const auto passphrase = MP_PASSPHRASE_HANDLER.generate_hash_for("passphrase");

    EXPECT_EQ(passphrase, "f28cb995d91eed8064674766f28e468aae8065b2cf02af556c857dd77de2d2476f3830fd02147f3e35037a1812df"
                          "0d0d0934fa677be585269fee5358d5c70758");
}

TEST(PassphraseHandler, generateHashErrorThrows)
{
    REPLACE(EVP_PBE_scrypt, [](auto...) { return 0; });

    MP_EXPECT_THROW_THAT(MP_PASSPHRASE_HANDLER.generate_hash_for("passphrase"), std::runtime_error,
                         mpt::match_what(StrEq("Cannot generate passphrase hash")));
}

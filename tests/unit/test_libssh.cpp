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

#include <multipass/ssh/libssh_wrapper.h>

#include <cstdlib>
#include <cstring>
#include <memory>

TEST(Libssh, eventNewReturnsNonNullAndCanBeFreed)
{
    ssh_event event = MP_LIBSSH.ssh_event_new();
    ASSERT_NE(event, nullptr);
    MP_LIBSSH.ssh_event_free(event); // must not crash
}

TEST(Libssh, pkiGenerateExportAndFreeRoundTrip)
{
    ssh_key key{nullptr};
    ASSERT_EQ(MP_LIBSSH.ssh_pki_generate(SSH_KEYTYPE_RSA, 2048, &key), SSH_OK);
    ASSERT_NE(key, nullptr);
    std::unique_ptr<ssh_key_struct, void (*)(ssh_key)> key_guard{key, [](ssh_key k) {
                                                                     MP_LIBSSH.ssh_key_free(k);
                                                                 }};

    char* b64{nullptr};
    ASSERT_EQ(MP_LIBSSH.ssh_pki_export_pubkey_base64(key_guard.get(), &b64), SSH_OK);
    ASSERT_NE(b64, nullptr);
    std::unique_ptr<char, decltype(&std::free)> b64_guard{b64, &std::free};
    EXPECT_GT(std::strlen(b64), 0u);
}

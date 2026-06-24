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

#include <multipass/ssh/libssh.h>

#include <cstring>

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

    char* b64{nullptr};
    EXPECT_EQ(MP_LIBSSH.ssh_pki_export_pubkey_base64(key, &b64), SSH_OK);
    ASSERT_NE(b64, nullptr);
    EXPECT_GT(std::strlen(b64), 0u);

    MP_LIBSSH.ssh_string_free_char(b64);
    MP_LIBSSH.ssh_key_free(key);
}

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

#include <QByteArray>
#include <QDataStream>

#include <cstdlib>
#include <cstring>
#include <memory>

TEST(Libssh, eventNewReturnsNonNullAndCanBeFreed)
{
    ssh_event event = MP_LIBSSH.ssh_event_new();
    ASSERT_NE(event, nullptr);
    MP_LIBSSH.ssh_event_free(event);
}

TEST(Libssh, pkiGenerateExportAndFreeRoundTrip)
{
    std::unique_ptr<ssh_pki_ctx_struct, void (*)(ssh_pki_ctx)> context{
        MP_LIBSSH.ssh_pki_ctx_new(),
        [](ssh_pki_ctx ctx) { MP_LIBSSH.ssh_pki_ctx_free(ctx); }};
    ASSERT_NE(context, nullptr);
    const int key_bits = 2048;
    ASSERT_EQ(
        MP_LIBSSH.ssh_pki_ctx_options_set(context.get(), SSH_PKI_OPTION_RSA_KEY_SIZE, &key_bits),
        SSH_OK);

    ssh_key key{nullptr};
    ASSERT_EQ(MP_LIBSSH.ssh_pki_generate_key(SSH_KEYTYPE_RSA, context.get(), &key), SSH_OK);
    ASSERT_NE(key, nullptr);
    std::unique_ptr<ssh_key_struct, void (*)(ssh_key)> key_guard{key, [](ssh_key k) {
                                                                     MP_LIBSSH.ssh_key_free(k);
                                                                 }};

    char* b64{nullptr};
    ASSERT_EQ(MP_LIBSSH.ssh_pki_export_pubkey_base64(key_guard.get(), &b64), SSH_OK);
    ASSERT_NE(b64, nullptr);
    std::unique_ptr<char, decltype(&std::free)> b64_guard{b64, &std::free};
    EXPECT_GT(std::strlen(b64), 0u);

    QDataStream public_key{QByteArray::fromBase64(b64)};
    QByteArray key_type, exponent, modulus;
    public_key >> key_type >> exponent >> modulus;
    ASSERT_EQ(public_key.status(), QDataStream::Ok);
    EXPECT_EQ(key_type, "ssh-rsa");
    ASSERT_EQ(modulus.size(), key_bits / 8 + 1);
    EXPECT_EQ(modulus.front(), '\0');
    EXPECT_NE(static_cast<unsigned char>(modulus[1]) & 0x80, 0);
}

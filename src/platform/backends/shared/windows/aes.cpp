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

#include "aes.h"

#include <multipass/utils.h>

#include <openssl/evp.h>

namespace mp = multipass;

using EVP_CIPHER_CTX_free_ptr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&::EVP_CIPHER_CTX_free)>;

mp::AES::AES(const Singleton<AES>::PrivatePass& pass) noexcept : Singleton<AES>::Singleton{pass}
{
}

int mp::AES::aes_256_key_size()
{
    return 32;
}

int mp::AES::aes_256_block_size()
{
    return 16;
}

void mp::AES::decrypt(const std::vector<uint8_t> key, const std::vector<uint8_t> iv, const std::string& ctext,
                      std::string& rtext)
{
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    if (!EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data()))
        throw std::runtime_error("EVP_DecryptInit_ex failed");

    rtext.resize(ctext.size());
    int out_len1 = (int)rtext.size();

    if (!EVP_DecryptUpdate(ctx.get(), (uint8_t*)&rtext[0], &out_len1, (const uint8_t*)&ctext[0], (int)ctext.size()))
        throw std::runtime_error("EVP_DecryptUpdate failed");

    int out_len2 = (int)rtext.size() - out_len1;
    if (!EVP_DecryptFinal_ex(ctx.get(), (uint8_t*)&rtext[0] + out_len1, &out_len2))
        throw std::runtime_error("EVP_DecryptFinal_ex failed");

    rtext.resize(out_len1 + out_len2);
}

void mp::AES::encrypt(const std::vector<uint8_t> key, const std::vector<uint8_t> iv, const std::string& ptext,
                      std::string& ctext)
{
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    if (!EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data()))
        throw std::runtime_error("EVP_EncryptInit_ex failed");

    ctext.resize(ptext.size() + iv.size());
    int out_len1 = (int)ctext.size();

    if (!EVP_EncryptUpdate(ctx.get(), (uint8_t*)&ctext[0], &out_len1, (const uint8_t*)&ptext[0], (int)ptext.size()))
        throw std::runtime_error("EVP_EncryptUpdate failed");

    int out_len2 = (int)ctext.size() - out_len1;
    if (!EVP_EncryptFinal_ex(ctx.get(), (uint8_t*)&ctext[0] + out_len1, &out_len2))
        throw std::runtime_error("EVP_EncryptFinal_ex failed");

    ctext.resize(out_len1 + out_len2);
}

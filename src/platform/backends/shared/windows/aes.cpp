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

namespace
{
constexpr int key_size = 32;
constexpr int block_size = 16;
} // namespace

mp::AES::AES(const Singleton<AES>::PrivatePass& pass) noexcept : Singleton<AES>::Singleton{pass}
{
}

int mp::AES::aes_256_key_size() const
{
    return key_size;
}

int mp::AES::aes_256_block_size() const
{
    return block_size;
}

std::string mp::AES::decrypt(const std::vector<uint8_t>& key,
                             const std::vector<uint8_t>& iv,
                             const std::string& encrypted_data) const
{
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    if (!EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data()))
        throw std::runtime_error("EVP_DecryptInit_ex failed");

    std::string decrypted_data;
    decrypted_data.resize(encrypted_data.size());
    int out_len1 = (int)decrypted_data.size();

    if (!EVP_DecryptUpdate(ctx.get(),
                           (uint8_t*)&decrypted_data[0],
                           &out_len1,
                           (const uint8_t*)&encrypted_data[0],
                           (int)encrypted_data.size()))
        throw std::runtime_error("EVP_DecryptUpdate failed");

    int out_len2 = (int)decrypted_data.size() - out_len1;
    if (!EVP_DecryptFinal_ex(ctx.get(), (uint8_t*)&decrypted_data[0] + out_len1, &out_len2))
        throw std::runtime_error("EVP_DecryptFinal_ex failed");

    decrypted_data.resize(out_len1 + out_len2);
    return decrypted_data;
}

std::string mp::AES::encrypt(const std::vector<uint8_t>& key,
                             const std::vector<uint8_t>& iv,
                             const std::string& data) const
{
    EVP_CIPHER_CTX_free_ptr ctx(EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free);
    if (!EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data()))
        throw std::runtime_error("EVP_EncryptInit_ex failed");

    std::string encrypted_data;
    encrypted_data.resize(data.size() + iv.size());
    int out_len1 = (int)encrypted_data.size();

    if (!EVP_EncryptUpdate(ctx.get(),
                           (uint8_t*)&encrypted_data[0],
                           &out_len1,
                           (const uint8_t*)&data[0],
                           (int)data.size()))
        throw std::runtime_error("EVP_EncryptUpdate failed");

    int out_len2 = (int)encrypted_data.size() - out_len1;
    if (!EVP_EncryptFinal_ex(ctx.get(), (uint8_t*)&encrypted_data[0] + out_len1, &out_len2))
        throw std::runtime_error("EVP_EncryptFinal_ex failed");

    encrypted_data.resize(out_len1 + out_len2);
    return encrypted_data;
}

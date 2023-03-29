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

#ifndef MULTIPASS_AES_H
#define MULTIPASS_AES_H

#include <multipass/singleton.h>

#include <string>
#include <vector>

#define MP_AES multipass::AES::instance()

namespace multipass
{
class AES : public Singleton<AES>
{
public:
    AES(const Singleton<AES>::PrivatePass&) noexcept;

    virtual int aes_256_key_size();
    virtual int aes_256_block_size();
    virtual void decrypt(const std::vector<uint8_t> key, const std::vector<uint8_t> iv, const std::string& ctext,
                         std::string& rtext);
    virtual void encrypt(const std::vector<uint8_t> key, const std::vector<uint8_t> iv, const std::string& ptext,
                         std::string& ctext);
};
} // namespace multipass

#endif // MULTIPASS_AES_H

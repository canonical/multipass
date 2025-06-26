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

#pragma once

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

    virtual int aes_256_key_size() const;
    virtual int aes_256_block_size() const;
    virtual std::string decrypt(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv,
                                const std::string& encrypted_data) const;
    virtual std::string encrypt(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv,
                                const std::string& data) const;
};
} // namespace multipass

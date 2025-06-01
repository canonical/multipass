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

#include "common.h"
#include "mock_singleton_helpers.h"

#include <src/platform/backends/shared/windows/aes.h>

namespace multipass::test
{
class MockAES : public AES
{
public:
    MockAES(const Singleton<AES>::PrivatePass& pass) noexcept : AES{pass}
    {
        ON_CALL(*this, aes_256_key_size).WillByDefault(testing::Return(AES::aes_256_key_size()));
        ON_CALL(*this, aes_256_block_size)
            .WillByDefault(testing::Return(AES::aes_256_block_size()));
    }

    MOCK_METHOD(int, aes_256_key_size, (), (const, override));
    MOCK_METHOD(int, aes_256_block_size, (), (const, override));
    MOCK_METHOD(std::string,
                decrypt,
                (const std::vector<uint8_t>&, const std::vector<uint8_t>&, const std::string&),
                (const, override));
    MOCK_METHOD(std::string,
                encrypt,
                (const std::vector<uint8_t>&, const std::vector<uint8_t>&, const std::string&),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockAES, AES);
};
} // namespace multipass::test

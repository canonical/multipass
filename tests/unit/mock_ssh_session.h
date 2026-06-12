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
#include "mock_ssh_process.h"

#include <multipass/ssh/ssh_session.h>

namespace multipass::test
{
struct MockSSHSession : public SSHSession
{
    MockSSHSession()
    {
        ON_CALL(*this, exec).WillByDefault(std::make_unique<testing::NiceMock<MockSSHProcess>>);
    }

    MOCK_METHOD(std::unique_ptr<SSHProcess>,
                exec,
                (const std::string& cmd, bool whisper),
                (override));
    MOCK_METHOD(bool, is_connected, (), (const, override));
    operator ssh_session() override
    {
        return nullptr;
    }
    MOCK_METHOD(void, force_shutdown, (), (override));
};
} // namespace multipass::test

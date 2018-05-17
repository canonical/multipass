/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "mock_ssh.h"
#include "stub_ssh_key_provider.h"

#include <multipass/ssh/ssh_session.h>

#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

TEST(SSHSession, throws_when_unable_to_allocate_session)
{
    REPLACE(ssh_new, []() { return nullptr; });
    EXPECT_THROW(mp::SSHSession("theanswertoeverything", 42), std::runtime_error);
}

TEST(SSHSession, throws_when_unable_to_set_option)
{
    REPLACE(ssh_options_set, [](auto...) { return SSH_ERROR; });
    EXPECT_THROW(mp::SSHSession("theanswertoeverything", 42), std::runtime_error);
}

TEST(SSHSession, throws_when_unable_to_connect)
{
    REPLACE(ssh_connect, [](auto...) { return SSH_ERROR; });
    EXPECT_THROW(mp::SSHSession("theanswertoeverything", 42), std::runtime_error);
}

TEST(SSHSession, throws_when_unable_to_auth)
{
    mp::test::StubSSHKeyProvider key_provider;
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    REPLACE(ssh_userauth_publickey, [](auto...) { return SSH_AUTH_ERROR; });
    EXPECT_THROW(mp::SSHSession("theanswertoeverything", 42, "ubuntu", key_provider), std::runtime_error);
}

TEST(SSHSession, exec_throws_on_a_dead_session)
{
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    mp::SSHSession session{"theanswertoeverything", 42};

    REPLACE(ssh_is_connected, [](auto...) { return false; });
    EXPECT_THROW(session.exec("dummy"), std::runtime_error);
}

TEST(SSHSession, exec_throws_if_ssh_is_dead)
{
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    mp::SSHSession session{"theanswertoeverything", 42};

    REPLACE(ssh_is_connected, [](auto...) { return false; });
    EXPECT_THROW(session.exec("dummy"), std::runtime_error);
}

TEST(SSHSession, exec_throws_when_unable_to_open_a_channel_session)
{
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    mp::SSHSession session{"theanswertoeverything", 42};

    REPLACE(ssh_is_connected, [](auto...) { return true; });
    REPLACE(ssh_channel_open_session, [](auto...) { return SSH_ERROR; });
    EXPECT_THROW(session.exec("dummy"), std::runtime_error);
}

TEST(SSHSession, exec_throws_when_unable_to_request_channel_exec)
{
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    mp::SSHSession session{"theanswertoeverything", 42};

    REPLACE(ssh_is_connected, [](auto...) { return true; });
    REPLACE(ssh_channel_open_session, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_exec, [](auto...) { return SSH_ERROR; });
    EXPECT_THROW(session.exec("dummy"), std::runtime_error);
}

TEST(SSHSession, exec_succeeds)
{
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    mp::SSHSession session{"theanswertoeverything", 42};

    REPLACE(ssh_is_connected, [](auto...) { return true; });
    REPLACE(ssh_channel_open_session, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_exec, [](auto...) { return SSH_OK; });

    EXPECT_NO_THROW(session.exec("dummy"));
}

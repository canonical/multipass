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
#include "disabling_macros.h"
#include "fake_key_data.h"
#include "mock_ssh_client.h"
#include "mock_ssh_test_fixture.h"
#include "stub_console.h"
#include "stub_ssh_key_provider.h"

#include <multipass/ssh/ssh_client.h>
#include <multipass/ssh/ssh_session.h>

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
struct SSHClient : public testing::Test
{
    mp::SSHClient make_ssh_client()
    {
        return {std::make_unique<mp::SSHSession>("a", 42, "ubuntu", key_provider), console_creator};
    }

    const mpt::StubSSHKeyProvider key_provider;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mp::SSHClient::ConsoleCreator console_creator = [](auto /*channel*/) {
        return std::make_unique<mpt::StubConsole>();
    };
};
} // namespace

TEST_F(SSHClient, standardCtorDoesNotThrow)
{
    EXPECT_NO_THROW(mp::SSHClient("a", 42, "foo", mpt::fake_key_data, console_creator));
}

TEST_F(SSHClient, execSingleCommandReturnsOKNoFailure)
{
    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = 0;
        return SSH_OK;
    });
    auto client = make_ssh_client();

    EXPECT_EQ(client.exec({"foo"}), SSH_OK);
}

TEST_F(SSHClient, execMultipleCommandsReturnsOKNoFailure)
{
    auto client = make_ssh_client();

    std::vector<std::vector<std::string>> commands{{"ls", "-la"}, {"pwd"}};
    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = 0;
        return SSH_OK;
    });
    EXPECT_EQ(client.exec(commands), SSH_OK);
}

TEST_F(SSHClient, execReturnsErrorCodeOnFailure)
{
    auto client = make_ssh_client();
    constexpr int failure_code{127};
    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = failure_code;
        return SSH_OK;
    });

    EXPECT_EQ(client.exec({"foo"}), failure_code);
}

TEST_F(SSHClient, DISABLE_ON_WINDOWS(execPollingWorksAsExpected))
{
    int poll_count{0};
    auto client = make_ssh_client();

    mock_ssh_test_fixture.is_eof.returnValue(0);

    auto event_dopoll = [this, &poll_count](auto...) {
        ++poll_count;
        mock_ssh_test_fixture.is_eof.returnValue(true);
        return SSH_OK;
    };

    REPLACE(ssh_channel_get_exit_state, [](ssh_channel_struct*, unsigned int* val, char**, int*) {
        *val = 0;
        return SSH_OK;
    });
    REPLACE(ssh_event_dopoll, event_dopoll);

    EXPECT_EQ(client.exec({"foo"}), SSH_OK);
    EXPECT_EQ(poll_count, 1);
}

TEST_F(SSHClient, throwsWhenUnableToOpenSession)
{
    REPLACE(ssh_channel_open_session, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(make_ssh_client(), std::runtime_error);
}

TEST_F(SSHClient, throwWhenRequestShellFails)
{
    auto client = make_ssh_client();
    REPLACE(ssh_channel_request_pty, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_change_pty_size, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_shell, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(client.connect(), std::runtime_error);
}

TEST_F(SSHClient, throwWhenRequestExecFails)
{
    auto client = make_ssh_client();
    REPLACE(ssh_channel_request_pty, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_change_pty_size, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_exec, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(client.exec({"foo"}), std::runtime_error);
}

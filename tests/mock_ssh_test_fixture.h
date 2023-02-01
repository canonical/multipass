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

#ifndef MULTIPASS_MOCK_SSH_TEST_FIXTURE_H
#define MULTIPASS_MOCK_SSH_TEST_FIXTURE_H

#include "mock_ssh.h"

namespace multipass
{
namespace test
{
// This sets up all default values for any libssh API calls.
// Use REPLACE() *after* instantiating this in the test unit to override the default.
struct MockSSHTestFixture
{
    MockSSHTestFixture()
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);
        userauth_publickey.returnValue(SSH_OK);
        request_exec.returnValue(SSH_OK);
        channel_read.returnValue(0);
        is_eof.returnValue(true);
        get_exit_status.returnValue(SSH_OK);
        channel_is_open.returnValue(true);
        channel_is_closed.returnValue(0);
        options_set.returnValue(SSH_OK);
    }

    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};
    decltype(MOCK(ssh_userauth_publickey)) userauth_publickey{MOCK(ssh_userauth_publickey)};
    decltype(MOCK(ssh_channel_request_exec)) request_exec{MOCK(ssh_channel_request_exec)};
    decltype(MOCK(ssh_channel_read_timeout)) channel_read{MOCK(ssh_channel_read_timeout)};
    decltype(MOCK(ssh_channel_is_eof)) is_eof{MOCK(ssh_channel_is_eof)};
    decltype(MOCK(ssh_channel_get_exit_status)) get_exit_status{MOCK(ssh_channel_get_exit_status)};
    decltype(MOCK(ssh_channel_is_open)) channel_is_open{MOCK(ssh_channel_is_open)};
    decltype(MOCK(ssh_channel_is_closed)) channel_is_closed{MOCK(ssh_channel_is_closed)};
    decltype(MOCK(ssh_options_set)) options_set{MOCK(ssh_options_set)};
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_SSH_TEST_FIXTURE_H

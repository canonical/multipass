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

#ifndef MULTIPASS_SFTP_SERVER_TEST_FIXTURE_H
#define MULTIPASS_SFTP_SERVER_TEST_FIXTURE_H

#include "mock_sftpserver.h"
#include "mock_ssh.h"

#include <gtest/gtest.h>

namespace multipass
{
namespace test
{
struct SftpServerTest : public testing::Test
{
    SftpServerTest()
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);
        request_exec.returnValue(SSH_OK);
        init_sftp.returnValue(SSH_OK);
        reply_status.returnValue(SSH_OK);
        get_client_msg.returnValue(nullptr);
    }

    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};
    decltype(MOCK(ssh_channel_request_exec)) request_exec{MOCK(ssh_channel_request_exec)};
    decltype(MOCK(sftp_server_init)) init_sftp{MOCK(sftp_server_init)};
    decltype(MOCK(sftp_free)) free_sftp{MOCK(sftp_free)};
    decltype(MOCK(sftp_reply_status)) reply_status{MOCK(sftp_reply_status)};
    decltype(MOCK(sftp_get_client_message)) get_client_msg{MOCK(sftp_get_client_message)};
    decltype(MOCK(sftp_client_message_free)) msg_free{MOCK(sftp_client_message_free)};
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_SFTP_SERVER_TEST_FIXTURE_H

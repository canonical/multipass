/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#include "mock_sftp.h"
#include "mock_sftpserver.h"
#include "mock_ssh.h"

#include <gtest/gtest.h>

namespace multipass
{
namespace test
{
class ExitStatusMock
{
public:
    ExitStatusMock()
    {
        add_channel_cbs = [this](ssh_channel, ssh_channel_callbacks cb) {
            channel_cbs = cb;
            return SSH_OK;
        };

        event_do_poll = [this](auto...) {
            if (channel_cbs == nullptr)
                return SSH_ERROR;
            channel_cbs->channel_exit_status_function(nullptr, nullptr, exit_code, channel_cbs->userdata);
            return SSH_OK;
        };
    }

    ~ExitStatusMock()
    {
        add_channel_cbs = std::move(old_add_channel_cbs);
        event_do_poll = std::move(old_event_do_poll);
    }

    void return_exit_code(int code)
    {
        exit_code = code;
    }

private:
    decltype(mock_ssh_add_channel_callbacks)& add_channel_cbs{mock_ssh_add_channel_callbacks};
    decltype(mock_ssh_add_channel_callbacks) old_add_channel_cbs{std::move(mock_ssh_add_channel_callbacks)};
    decltype(mock_ssh_event_dopoll)& event_do_poll{mock_ssh_event_dopoll};
    decltype(mock_ssh_event_dopoll) old_event_do_poll{std::move(mock_ssh_event_dopoll)};

    int exit_code{SSH_OK};
    ssh_channel_callbacks channel_cbs{nullptr};
};

struct SftpServerTest : public testing::Test
{
    SftpServerTest()
        : free_sftp{mock_sftp_free, [](sftp_session sftp) {
                        std::free(sftp->handles);
                        std::free(sftp);
                    }}
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);
        request_exec.returnValue(SSH_OK);
        init_sftp.returnValue(SSH_OK);
        reply_status.returnValue(SSH_OK);
        get_client_msg.returnValue(nullptr);
        handle_sftp.returnValue(nullptr);
    }

    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};
    decltype(MOCK(ssh_channel_request_exec)) request_exec{MOCK(ssh_channel_request_exec)};
    decltype(MOCK(sftp_server_init)) init_sftp{MOCK(sftp_server_init)};
    decltype(MOCK(sftp_reply_status)) reply_status{MOCK(sftp_reply_status)};
    decltype(MOCK(sftp_get_client_message)) get_client_msg{MOCK(sftp_get_client_message)};
    decltype(MOCK(sftp_client_message_free)) msg_free{MOCK(sftp_client_message_free)};
    decltype(MOCK(sftp_handle)) handle_sftp{MOCK(sftp_handle)};
    MockScope<decltype(mock_sftp_free)> free_sftp;
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_SFTP_SERVER_TEST_FIXTURE_H

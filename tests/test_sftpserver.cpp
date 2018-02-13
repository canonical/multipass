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

#include "sftp_server_test_fixture.h"

#include <multipass/ssh/ssh_session.h>
#include <multipass/sshfs_mount/sftp_server.h>

#include <gmock/gmock.h>

#include <queue>

namespace mp = multipass;
using namespace testing;

namespace
{
struct SftpServer : public mp::test::SftpServerTest
{
    mp::SftpServer make_sftpserver()
    {
        mp::SSHSession session{"a", 42};
        auto proc = session.exec("sshfs");
        return {std::move(session), std::move(proc), default_map, default_map, default_id, default_id, nullstream};
    }

    auto make_msg()
    {
        auto msg = std::make_unique<sftp_client_message_struct>();
        msg->type = 0;
        messages.push(msg.get());
        return msg;
    }

    auto make_msg_handler()
    {
        auto msg_handler = [this](auto...) -> sftp_client_message {
            if (messages.empty())
                return nullptr;
            auto msg = messages.front();
            messages.pop();
            return msg;
        };
        return msg_handler;
    }

    std::queue<sftp_client_message> messages;
    std::unordered_map<int, int> default_map;
    int default_id{1000};
    std::stringstream nullstream;
};
} // namespace

TEST_F(SftpServer, throws_when_failed_to_init)
{
    REPLACE(sftp_server_init, [](auto...) { return SSH_ERROR; });
    EXPECT_THROW(make_sftpserver(), std::runtime_error);
}

TEST_F(SftpServer, stops_after_a_null_message)
{
    auto sftp = make_sftpserver();

    REPLACE(sftp_get_client_message, [](auto...) { return nullptr; });
    sftp.run();
}

TEST_F(SftpServer, frees_message)
{
    auto sftp = make_sftpserver();

    auto msg = make_msg();

    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    msg_free.expectCalled(1).withValues(msg.get());
}

TEST_F(SftpServer, replies_bad_op_on_invalid_message_type)
{
    auto sftp = make_sftpserver();

    auto msg = make_msg();
    const uint8_t bad_type = 255;
    msg->type = bad_type;

    REPLACE(sftp_get_client_message, make_msg_handler());

    bool values_as_expected{false};
    int num_calls{0};
    auto reply_status = [&msg, &values_as_expected, &num_calls](sftp_client_message cmsg, uint32_t status,
                                                                const char*) {
        values_as_expected = (msg.get() == cmsg) && (status == SSH_FX_OP_UNSUPPORTED);
        ++num_calls;
        return SSH_OK;
    };

    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
    EXPECT_TRUE(values_as_expected);
}

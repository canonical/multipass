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

#include <multipass/ssh/ssh_session.h>

#include <gmock/gmock.h>

#include <algorithm>

namespace mp = multipass;
using namespace testing;

namespace
{
struct SSHProcess : public Test
{
    SSHProcess()
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);
        request_exec.returnValue(SSH_OK);
    }
    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};
    decltype(MOCK(ssh_channel_request_exec)) request_exec{MOCK(ssh_channel_request_exec)};
    mp::SSHSession session{"theanswertoeverything", 42};
};
} // namespace

TEST_F(SSHProcess, can_retrieve_exit_status)
{
    int expected_status = 42;
    REPLACE(ssh_channel_get_exit_status, [&expected_status](auto...) { return expected_status; });

    auto proc = session.exec("something");
    EXPECT_THAT(proc.exit_code(), Eq(expected_status));
}

TEST_F(SSHProcess, specifies_stderr_correctly)
{
    int expected_is_stderr = 0;
    auto channel_read = [&expected_is_stderr](ssh_channel, void*, uint32_t, int is_stderr, int) {
        EXPECT_THAT(expected_is_stderr, Eq(is_stderr));
        return 0;
    };
    REPLACE(ssh_channel_read_timeout, channel_read);

    auto proc = session.exec("something");
    proc.read_std_output();

    expected_is_stderr = 1;
    proc.read_std_error();
}

TEST_F(SSHProcess, throws_on_read_errors)
{
    REPLACE(ssh_channel_read_timeout, [](auto...) { return -1; });

    auto proc = session.exec("something");
    EXPECT_THROW(proc.read_std_output(), std::runtime_error);
}

TEST_F(SSHProcess, read_std_output_returns_empty_string_on_eof)
{
    REPLACE(ssh_channel_read_timeout, [](auto...) { return 0; });

    auto proc = session.exec("something");
    auto output = proc.read_std_output();

    EXPECT_TRUE(output.empty());
}

TEST_F(SSHProcess, can_read_output)
{
    std::string expected_output{"some content here"};
    auto remaining = expected_output.size();
    auto channel_read = [&expected_output, &remaining](ssh_channel, void* dest, uint32_t count, int is_stderr, int) {
        const auto num_to_copy = std::min(count, static_cast<uint32_t>(remaining));
        std::copy_n(expected_output.begin(), num_to_copy, reinterpret_cast<char*>(dest));
        remaining -= num_to_copy;
        return num_to_copy;
    };
    REPLACE(ssh_channel_read_timeout, channel_read);

    auto proc = session.exec("something");
    auto output = proc.read_std_output();

    EXPECT_THAT(output, StrEq(expected_output));
}

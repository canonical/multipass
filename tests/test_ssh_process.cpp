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
#include "mock_ssh_test_fixture.h"
#include "stub_ssh_key_provider.h"

#include <multipass/ssh/ssh_session.h>

#include <algorithm>
#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct SSHProcess : public Test
{
    const mpt::StubSSHKeyProvider key_provider;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mp::SSHSession session{"theanswertoeverything", 42, "ubuntu", key_provider};
};
} // namespace

TEST_F(SSHProcess, can_retrieve_exit_status)
{
    ssh_channel_callbacks callbacks{nullptr};
    auto add_channel_cbs = [&callbacks](ssh_channel, ssh_channel_callbacks cb) {
        callbacks = cb;
        return SSH_OK;
    };
    REPLACE(ssh_add_channel_callbacks, add_channel_cbs);

    int expected_status{42};
    auto event_dopoll = [&callbacks, &expected_status](auto...) {
        if (!callbacks)
            return SSH_ERROR;
        callbacks->channel_exit_status_function(nullptr, nullptr, expected_status, callbacks->userdata);
        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, event_dopoll);
    auto proc = session.exec("something");
    EXPECT_THAT(proc.exit_code(), Eq(expected_status));
}

TEST_F(SSHProcess, exit_code_times_out)
{
    REPLACE(ssh_event_dopoll, [](ssh_event, int timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout + 1));
        return SSH_OK;
    });
    auto proc = session.exec("something");
    EXPECT_THROW(proc.exit_code(std::chrono::milliseconds(1)), std::runtime_error);
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

TEST_F(SSHProcess, reading_output_returns_empty_if_channel_closed)
{
    REPLACE(ssh_channel_is_closed, [](auto...) { return 1; });

    auto proc = session.exec("something");
    auto output = proc.read_std_output();
    EXPECT_TRUE(output.empty());
}

TEST_F(SSHProcess, reading_failure_returns_empty_if_channel_closed)
{
    int channel_closed{0};
    REPLACE(ssh_channel_read_timeout, [&channel_closed](auto...) {
        channel_closed = 1;
        return -1;
    });
    REPLACE(ssh_channel_is_closed, [&channel_closed](auto...) { return channel_closed; });

    auto proc = session.exec("something");
    auto output = proc.read_std_output();
    EXPECT_TRUE(output.empty());
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
        const auto begin = expected_output.begin() + expected_output.size() - remaining;
        std::copy_n(begin, num_to_copy, reinterpret_cast<char*>(dest));
        remaining -= num_to_copy;
        return num_to_copy;
    };
    REPLACE(ssh_channel_read_timeout, channel_read);

    auto proc = session.exec("something");
    auto output = proc.read_std_output();

    EXPECT_THAT(output, StrEq(expected_output));
}

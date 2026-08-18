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
#include "mock_ssh_callback_engine.h"
#include "mock_ssh_test_fixture.h"
#include "stub_ssh_key_provider.h"

#include <multipass/ssh/plain_ssh_session.h>

#include <algorithm>
#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct SSHProcess : public Test
{
    SSHProcess()
    {
        ON_CALL(mock_libssh, ssh_is_connected).WillByDefault(Return(1));
        ON_CALL(mock_libssh, ssh_channel_new).WillByDefault([](auto...) {
            return reinterpret_cast<ssh_channel>(0xdeadbeefdeadbeef);
        });
        ON_CALL(mock_libssh, ssh_event_new).WillByDefault([](auto...) {
            return reinterpret_cast<ssh_event>(0xdeadbeefdeadbeef);
        });
        ON_CALL(mock_libssh, ssh_event_add_session).WillByDefault([](auto...) { return SSH_OK; });
        callback_mock_engine.push_state(callback_mock_engine.channel_exit_success);
    }
    const mpt::StubSSHKeyProvider key_provider;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mp::PlainSSHSession session{"theanswertoeverything", 42, "ubuntu", key_provider};
    mpt::MockLibssh::GuardedMock libssh_guard{mpt::MockLibssh::inject()};
    mpt::MockLibssh& mock_libssh = *libssh_guard.first;
    mpt::CallbackChEngineMock callback_mock_engine{mock_libssh};
};
} // namespace

TEST_F(SSHProcess, canRetrieveExitStatus)
{
    static constexpr int expected_status{42};
    mpt::CallbackChState cb_state{};
    cb_state.exit_code = expected_status;
    callback_mock_engine.push_state(cb_state);
    callback_mock_engine.pop_state();

    auto proc = session.exec("something");
    EXPECT_THAT(proc->exit_code(), Eq(expected_status));
}

TEST_F(SSHProcess, exitCodeTimesOut)
{
    EXPECT_CALL(mock_libssh, ssh_event_dopoll).WillRepeatedly([](ssh_event, int timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout + 1));
        return SSH_OK;
    });
    auto proc = session.exec("something");
    EXPECT_THROW(proc->exit_code(std::chrono::milliseconds(1)), std::runtime_error);
}

TEST_F(SSHProcess, specifiesStderrCorrectly)
{
    int expected_is_stderr = 0;
    auto channel_read = [&expected_is_stderr](ssh_channel, void*, uint32_t, int is_stderr, int) {
        EXPECT_THAT(expected_is_stderr, Eq(is_stderr));
        return 0;
    };
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout).WillRepeatedly(channel_read);

    auto proc = session.exec("something");
    proc->read_std_output();

    expected_is_stderr = 1;
    proc->read_std_error();
}

TEST_F(SSHProcess, readingOutputReturnsEmptyIfChannelClosed)
{
    auto proc = session.exec("something");
    auto output = proc->read_std_output();
    EXPECT_TRUE(output.empty());
}

TEST_F(SSHProcess, readingFailureReturnsEmptyIfChannelClosed)
{
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout).WillRepeatedly([](auto...) {
        MP_LIBSSH.ssh_event_dopoll(nullptr, 0);
        return -1;
    });

    auto proc = session.exec("something");
    auto output = proc->read_std_output();
    EXPECT_TRUE(output.empty());
}

TEST_F(SSHProcess, throwsOnReadErrors)
{
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout).WillOnce([](auto...) { return -1; });

    auto proc = session.exec("something");
    EXPECT_THROW(proc->read_std_output(), std::runtime_error);
}

TEST_F(SSHProcess, readStdOutputReturnsEmptyStringOnEof)
{
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout).WillOnce([](auto...) { return 0; });

    auto proc = session.exec("something");
    auto output = proc->read_std_output();

    EXPECT_TRUE(output.empty());
}

TEST_F(SSHProcess, canReadOutput)
{
    std::string expected_output{"some content here"};
    auto remaining = expected_output.size();
    auto channel_read = [&expected_output,
                         &remaining](ssh_channel, void* dest, uint32_t count, int, int) {
        const auto num_to_copy = std::min(count, static_cast<uint32_t>(remaining));
        const auto begin = expected_output.begin() + expected_output.size() - remaining;
        std::copy_n(begin, num_to_copy, reinterpret_cast<char*>(dest));
        remaining -= num_to_copy;
        return num_to_copy;
    };
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout).WillRepeatedly(channel_read);

    auto proc = session.exec("something");
    auto output = proc->read_std_output();

    EXPECT_THAT(output, StrEq(expected_output));
}

TEST_F(SSHProcess, getCmdReturnsCommandName)
{
    static constexpr auto* cmd = "my-command";
    auto proc = session.exec(cmd);
    EXPECT_EQ(proc->get_cmd(), cmd);
}

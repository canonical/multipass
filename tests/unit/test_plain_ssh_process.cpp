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
#include "mock_libssh.h"
#include "stub_ssh_key_provider.h"

#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/plain_ssh_session.h>

#include <libssh/callbacks.h>

#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <type_traits>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{

static_assert(!std::is_copy_constructible_v<mp::PlainSSHProcess>);
static_assert(!std::is_copy_assignable_v<mp::PlainSSHProcess>);
static_assert(!std::is_move_constructible_v<mp::PlainSSHProcess>);
static_assert(!std::is_move_assignable_v<mp::PlainSSHProcess>);
static_assert(!std::is_copy_constructible_v<mp::SSHProcess>);
static_assert(!std::is_copy_assignable_v<mp::SSHProcess>);

struct TestPlainSSHProcess : public Test
{
    TestPlainSSHProcess()
    {
        // Fully mock libssh so that a genuine PlainSSHSession (and therefore a genuine
        // PlainSSHProcess) can be constructed and driven end-to-end.
        ON_CALL(mock_libssh, ssh_new()).WillByDefault(Return(fake_session));
        ON_CALL(mock_libssh, ssh_options_set).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_connect).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_userauth_publickey).WillByDefault(Return(SSH_AUTH_SUCCESS));
        ON_CALL(mock_libssh, ssh_is_connected).WillByDefault(Return(1));
        ON_CALL(mock_libssh, ssh_get_fd).WillByDefault(Return(-1)); // no socket to shutdown

        // Channel creation defaults (make_channel).
        ON_CALL(mock_libssh, ssh_channel_new).WillByDefault(Return(fake_channel));
        ON_CALL(mock_libssh, ssh_channel_open_session).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_channel_request_exec).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_channel_is_closed).WillByDefault(Return(0));

        // Exit-status callback / event-loop defaults (read_exit_code).
        // TODO@vsock: replace with CallbackEngine after rebase on main
        ON_CALL(mock_libssh, ssh_add_channel_callbacks)
            .WillByDefault([this](ssh_channel, ssh_channel_callbacks cb) {
                exit_cb = cb; // captured so tests can fire the exit-status callback
                return SSH_OK;
            });
        ON_CALL(mock_libssh, ssh_event_new()).WillByDefault(Return(fake_event));
        ON_CALL(mock_libssh, ssh_event_add_session).WillByDefault(Return(SSH_OK));

        ON_CALL(mock_libssh, ssh_get_error).WillByDefault(Return("mocked error"));
    }

    mp::PlainSSHProcess make_ssh_process(const std::string& cmd = "cmd")
    {
        return mp::PlainSSHProcess{fake_session, cmd, std::unique_lock{mutex}};
    }

    mp::PlainSSHSession make_ssh_session() const
    {
        return mp::PlainSSHSession{"host", 42, "ubuntu", key_provider};
    }

    // Fires the captured exit-status callback with the given code, simulating libssh delivering
    // the remote process' exit status while polling the event loop.
    void deliver_exit_status(int exit_status)
    {
        ASSERT_THAT(exit_cb, NotNull());
        ASSERT_THAT(exit_cb->channel_exit_status_function, NotNull());
        exit_cb->channel_exit_status_function(fake_session,
                                              fake_channel,
                                              exit_status,
                                              exit_cb->userdata);
    }

    mpt::MockLibssh::GuardedMock guarded_mock = mpt::MockLibssh::inject<NiceMock>();
    mpt::MockLibssh& mock_libssh = *guarded_mock.first;

    constexpr static auto bad_addr = 0xdeadbeefdeadbeefull; // should reliably segfault on 32/64-bit
    ssh_session fake_session = reinterpret_cast<ssh_session>(bad_addr);
    ssh_channel fake_channel = reinterpret_cast<ssh_channel>(bad_addr);
    ssh_event fake_event = reinterpret_cast<ssh_event>(bad_addr);

    ssh_channel_callbacks exit_cb = nullptr;

    mpt::StubSSHKeyProvider key_provider;
    std::mutex mutex;
};
} // namespace

TEST_F(TestPlainSSHProcess, execThrowsOnADeadSession)
{
    EXPECT_CALL(mock_libssh, ssh_is_connected(fake_session)).WillOnce(Return(0));

    MP_EXPECT_THROW_THAT(make_ssh_process(),
                         mp::SSHException,
                         mpt::match_what(HasSubstr("not connected")));
}

TEST_F(TestPlainSSHProcess, execThrowsWhenUnableToOpenAChannelSession)
{
    constexpr auto err = "mocked error";
    EXPECT_CALL(mock_libssh, ssh_channel_open_session).WillOnce(Return(SSH_ERROR));
    EXPECT_CALL(mock_libssh, ssh_get_error(fake_session)).WillOnce(Return(err));

    MP_EXPECT_THROW_THAT(make_ssh_process(), mp::SSHException, mpt::match_what(HasSubstr(err)));
}

TEST_F(TestPlainSSHProcess, execThrowsWhenUnableToRequestChannelExec)
{
    constexpr auto err = "mocked error";
    ON_CALL(mock_libssh, ssh_channel_open_session).WillByDefault(Return(SSH_OK));
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec).WillOnce(Return(SSH_ERROR));
    EXPECT_CALL(mock_libssh, ssh_get_error(fake_session)).WillOnce(Return(err));

    MP_EXPECT_THROW_THAT(make_ssh_process(), mp::SSHException, mpt::match_what(HasSubstr(err)));
}

TEST_F(TestPlainSSHProcess, getCmdReturnsTheGivenCommand)
{
    auto proc = make_ssh_process("ls -la /tmp");
    EXPECT_EQ(proc.get_cmd(), "ls -la /tmp");
}

TEST_F(TestPlainSSHProcess, execPlainOnSessionProducesRunningProcess)
{
    auto session = make_ssh_session();
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq("uptime")))
        .WillOnce(Return(SSH_OK));

    auto proc = session.exec_plain("uptime");

    ASSERT_THAT(proc, NotNull());
    EXPECT_EQ(proc->get_cmd(), "uptime");
}

TEST_F(TestPlainSSHProcess, exitCodeReturnsDeliveredStatus)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_event_dopoll(fake_event, _)).WillOnce(InvokeWithoutArgs([this] {
        deliver_exit_status(42);
        return SSH_OK;
    }));

    EXPECT_EQ(proc.exit_code(), 42);
}

TEST_F(TestPlainSSHProcess, exitCodeIsCachedAcrossCalls)
{
    auto proc = make_ssh_process();

    // The event loop must only be set up once; the second call returns the cached value.
    EXPECT_CALL(mock_libssh, ssh_event_new()).WillOnce(Return(fake_event));
    EXPECT_CALL(mock_libssh, ssh_event_dopoll(fake_event, _)).WillOnce(InvokeWithoutArgs([this] {
        deliver_exit_status(7);
        return SSH_OK;
    }));

    EXPECT_EQ(proc.exit_code(), 7);
    EXPECT_EQ(proc.exit_code(), 7); // cached, no further polling
}

TEST_F(TestPlainSSHProcess, exitRecognizedReturnsTrueWhenStatusDelivered)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_event_dopoll(fake_event, _)).WillOnce(InvokeWithoutArgs([this] {
        deliver_exit_status(0);
        return SSH_OK;
    }));

    EXPECT_TRUE(proc.exit_recognized());
    EXPECT_EQ(proc.exit_code(), 0); // cached without another poll
}

TEST_F(TestPlainSSHProcess, exitRecognizedReturnsFalseOnTimeout)
{
    auto proc = make_ssh_process();

    // Poll never delivers a status; the deadline elapses and no exception escapes.
    ON_CALL(mock_libssh, ssh_event_dopoll).WillByDefault(Return(SSH_OK));

    EXPECT_FALSE(proc.exit_recognized(std::chrono::milliseconds(1)));
}

TEST_F(TestPlainSSHProcess, exitCodeTimesOut)
{
    auto proc = make_ssh_process();

    ON_CALL(mock_libssh, ssh_event_dopoll).WillByDefault(Return(SSH_OK));

    MP_EXPECT_THROW_THAT(proc.exit_code(std::chrono::milliseconds(1)),
                         mp::SSHProcessTimeoutException,
                         mpt::match_what(HasSubstr("timed out")));
}

TEST_F(TestPlainSSHProcess, exitCodeThrowsWhenEventCannotBeAllocated)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_event_new()).WillOnce(Return(nullptr));

    MP_EXPECT_THROW_THAT(proc.exit_code(),
                         mp::SSHProcessExitError,
                         mpt::match_what(HasSubstr("could not allocate event")));
}

TEST_F(TestPlainSSHProcess, exitCodeThrowsWhenCallbackCannotBeRegistered)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_add_channel_callbacks).WillOnce(Return(SSH_ERROR));

    MP_EXPECT_THROW_THAT(proc.exit_code(),
                         mp::SSHProcessExitError,
                         mpt::match_what(HasSubstr("could not register callback")));
}

TEST_F(TestPlainSSHProcess, exitCodeThrowsWhenEventCannotBeAddedToSession)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_event_add_session).WillOnce(Return(SSH_ERROR));

    MP_EXPECT_THROW_THAT(proc.exit_code(),
                         mp::SSHProcessExitError,
                         mpt::match_what(HasSubstr("could not add event to session")));
}

TEST_F(TestPlainSSHProcess, exitCodeThrowsWhenPollFails)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_event_dopoll(fake_event, _)).WillOnce(Return(SSH_ERROR));

    MP_EXPECT_THROW_THAT(proc.exit_code(),
                         mp::SSHProcessExitError,
                         mpt::match_what(HasSubstr("ssh_event_dopoll failed")));
}

TEST_F(TestPlainSSHProcess, exitCodeRethrowsSavedException)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_event_new()).WillOnce(Return(nullptr));

    MP_EXPECT_THROW_THAT(proc.exit_code(), mp::SSHProcessExitError, _);
    // A subsequent call rethrows the saved exception without touching the event loop again.
    MP_EXPECT_THROW_THAT(proc.exit_code(),
                         mp::SSHProcessExitError,
                         mpt::match_what(HasSubstr("could not allocate event")));
}

TEST_F(TestPlainSSHProcess, readStdOutputReturnsChannelData)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout(fake_channel, _, _, 0, _))
        .WillOnce(Invoke([](ssh_channel, void* dest, uint32_t, int, int) {
            constexpr auto data = "hello world";
            std::memcpy(dest, data, std::strlen(data));
            return static_cast<int>(std::strlen(data));
        }))
        .WillOnce(Return(0));

    EXPECT_EQ(proc.read_std_output(), "hello world");
}

TEST_F(TestPlainSSHProcess, readStdErrorReadsFromStderrStream)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout(fake_channel, _, _, 1, _))
        .WillOnce(Invoke([](ssh_channel, void* dest, uint32_t, int, int) {
            constexpr auto data = "boom";
            std::memcpy(dest, data, std::strlen(data));
            return static_cast<int>(std::strlen(data));
        }))
        .WillOnce(Return(0));

    EXPECT_EQ(proc.read_std_error(), "boom");
}

TEST_F(TestPlainSSHProcess, readStreamReturnsEmptyWhenChannelClosed)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_channel_is_closed(fake_channel)).WillOnce(Return(1));
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout).Times(0);

    EXPECT_EQ(proc.read_std_output(), std::string());
}

TEST_F(TestPlainSSHProcess, readStreamReturnsAccumulatedDataWhenChannelClosesMidRead)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout(fake_channel, _, _, 0, _))
        .WillOnce(Invoke([](ssh_channel, void* dest, uint32_t, int, int) {
            constexpr auto data = "partial";
            std::memcpy(dest, data, std::strlen(data));
            return static_cast<int>(std::strlen(data));
        }))
        .WillOnce(Return(-1)); // error return

    // On a negative read, a closed channel means EOF rather than an error.
    EXPECT_CALL(mock_libssh, ssh_channel_is_closed(fake_channel))
        .WillOnce(Return(0))  // initial guard
        .WillOnce(Return(1)); // after the negative read

    EXPECT_EQ(proc.read_std_output(), "partial");
}

TEST_F(TestPlainSSHProcess, readStreamThrowsOnReadErrorWithOpenChannel)
{
    auto proc = make_ssh_process();

    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout(fake_channel, _, _, 0, _))
        .WillOnce(Return(-5));
    ON_CALL(mock_libssh, ssh_channel_is_closed).WillByDefault(Return(0));

    MP_EXPECT_THROW_THAT(proc.read_std_output(),
                         mp::SSHException,
                         mpt::match_what(HasSubstr("error while reading ssh channel")));
}

TEST_F(TestPlainSSHProcess, readStdOutputReturnsEmptyStringOnEof)
{
    auto proc = make_ssh_process();

    // A zero-byte read signals EOF, so nothing is accumulated.
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout(fake_channel, _, _, 0, _))
        .WillOnce(Return(0));

    EXPECT_TRUE(proc.read_std_output().empty());
}

TEST_F(TestPlainSSHProcess, readStreamReturnsEmptyWhenReadFailsOnClosedChannel)
{
    auto proc = make_ssh_process();

    // The very first read fails, but because the channel has since closed it is treated as EOF
    // rather than an error, yielding empty output.
    int channel_closed{0};
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout(fake_channel, _, _, 0, _))
        .WillOnce(Invoke([&channel_closed](ssh_channel, void*, uint32_t, int, int) {
            channel_closed = 1;
            return -1;
        }));
    ON_CALL(mock_libssh, ssh_channel_is_closed).WillByDefault([&channel_closed](ssh_channel) {
        return channel_closed;
    });

    EXPECT_TRUE(proc.read_std_output().empty());
}

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
#include "mock_libssh.h"
#include "mock_logger.h"
#include "mock_terminal.h"
#include "stub_ssh_key_provider.h"
#include "stub_terminal.h"

#include <multipass/cli/return_codes.h>
#include <multipass/ssh/client_ssh_factory.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/ssh/platform_ssh_client.h>

#include <sstream>

#ifndef _WIN32
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mpl = multipass::logging;
using namespace testing;

namespace
{
constexpr auto vm_ok = static_cast<mp::VMReturnCode>(0);

struct SSHClient : public Test
{
    SSHClient()
    {
        ON_CALL(mock_libssh, ssh_new()).WillByDefault(Return(fake_session));
        ON_CALL(mock_libssh, ssh_options_set).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_connect).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_userauth_publickey).WillByDefault(Return(SSH_AUTH_SUCCESS));
        ON_CALL(mock_libssh, ssh_is_connected).WillByDefault(Return(1));
        ON_CALL(mock_libssh, ssh_get_fd).WillByDefault(Return(-1)); // no socket to shutdown
        ON_CALL(mock_libssh, ssh_get_error).WillByDefault(Return("mocked error"));
        ON_CALL(mock_libssh, ssh_channel_new).WillByDefault(Return(fake_channel));
        ON_CALL(mock_libssh, ssh_channel_open_session).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_channel_request_exec).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_channel_request_shell).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_channel_is_open).WillByDefault(Return(1));
        ON_CALL(mock_libssh, ssh_channel_is_eof).WillByDefault(Return(1));
        expect_exit_state(0);
    }

    void expect_exit_state(uint32_t code, char* signal = nullptr)
    {
        ON_CALL(mock_libssh, ssh_channel_get_exit_state)
            .WillByDefault(DoAll(SetArgPointee<1>(code), SetArgPointee<2>(signal), Return(SSH_OK)));
    }

    mp::PlainSSHSession make_ssh_session() const
    {
        mp::SSHCoordinates coord{"ubuntu",
                                 key_provider.private_key_as_base64(),
                                 42,
                                 "theanswertoeverything",
                                 {}};
        return mp::PlainSSHSession{coord};
    }

    std::unique_ptr<mp::PlatformSSHClient> make_ssh_client()
    {
        return std::make_unique<mp::PlatformSSHClient>(make_ssh_session(), &term);
    }

    mpt::MockLibssh::GuardedMock guarded_mock = mpt::MockLibssh::inject<NiceMock>();
    mpt::MockLibssh& mock_libssh = *guarded_mock.first;

    constexpr static auto bad_addr = 0xdeadbeefdeadbeefull; // should reliably segfault on 32/64-bit
    ssh_session fake_session = reinterpret_cast<ssh_session>(bad_addr);
    ssh_channel fake_channel = reinterpret_cast<ssh_channel>(bad_addr);

    std::stringstream trash_stream;
    mpt::StubTerminal term{trash_stream, trash_stream, trash_stream};
    mpt::StubSSHKeyProvider key_provider;
};
} // namespace

TEST_F(SSHClient, standardCtorDoesNotThrow)
{
    EXPECT_NO_THROW(make_ssh_client());
}

TEST_F(SSHClient, clientFactoryMakesPlatformSSHClient)
{
    mp::SSHCoordinates coord{"ubuntu", key_provider.private_key_as_base64(), 42, "host", {}};

    auto client = MP_CLIENT_SSH_FACTORY.make_ssh_client(coord, &term);

    EXPECT_THAT(dynamic_cast<mp::PlatformSSHClient*>(client.get()), NotNull());
}

TEST_F(SSHClient, secondClientCannotBeCreatedWhileFirstIsAlive)
{
    auto client = make_ssh_client();

    EXPECT_THROW(make_ssh_client(), std::logic_error);
}

TEST_F(SSHClient, clientCanBeCreatedAgainAfterPreviousIsDestroyed)
{
    make_ssh_client();

    EXPECT_NO_THROW(make_ssh_client());
}

TEST_F(SSHClient, execSingleCommandReturnsOKNoFailure)
{
    auto client = make_ssh_client();

    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, HasSubstr("foo")))
        .WillOnce(Return(SSH_OK));

    EXPECT_EQ(client->exec({{"foo"}}), vm_ok);
}

TEST_F(SSHClient, execMultipleCommandsChainsThemInOneRequest)
{
    auto client = make_ssh_client();

    EXPECT_CALL(mock_libssh,
                ssh_channel_request_exec(fake_channel, AllOf(HasSubstr("foo"), HasSubstr("&&"))))
        .WillOnce(Return(SSH_OK));

    EXPECT_EQ(client->exec({{"foo"}, {"bar"}}), vm_ok);
}

TEST_F(SSHClient, execReturnsErrorCodeOnFailure)
{
    constexpr int failure_code{1};
    auto client = make_ssh_client();
    expect_exit_state(failure_code);

    EXPECT_EQ(client->exec({{"foo"}}), static_cast<mp::VMReturnCode>(failure_code));
}

TEST_F(SSHClient, DISABLE_ON_WINDOWS(execPollingWorksAsExpected))
{
    auto client = make_ssh_client();

    EXPECT_CALL(mock_libssh, ssh_channel_is_eof(fake_channel))
        .WillOnce(Return(0))
        .WillRepeatedly(Return(1));
    EXPECT_CALL(mock_libssh, ssh_event_dopoll).Times(1).WillOnce(Return(SSH_OK));

    EXPECT_EQ(client->exec({{"foo"}}), vm_ok);
}

TEST_F(SSHClient, throwsWhenUnableToOpenSession)
{
    EXPECT_CALL(mock_libssh, ssh_channel_open_session(fake_channel)).WillOnce(Return(SSH_ERROR));

    EXPECT_THROW(make_ssh_client(), std::runtime_error);
}

TEST_F(SSHClient, openShellRequestsAShell)
{
    auto client = make_ssh_client();

    EXPECT_CALL(mock_libssh, ssh_channel_request_shell(fake_channel)).WillOnce(Return(SSH_OK));
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec).Times(0);

    EXPECT_EQ(client->open_shell(), vm_ok);
}

TEST_F(SSHClient, throwWhenRequestShellFails)
{
    auto client = make_ssh_client();
    EXPECT_CALL(mock_libssh, ssh_channel_request_shell).WillOnce(Return(SSH_ERROR));

    EXPECT_THROW(client->open_shell(), std::runtime_error);
}

TEST_F(SSHClient, throwWhenRequestExecFails)
{
    auto client = make_ssh_client();
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec).WillOnce(Return(SSH_ERROR));

    EXPECT_THROW(client->exec({{"foo"}}), std::runtime_error);
}

TEST_F(SSHClient, throwWhenExitStateCannotBeObtained)
{
    auto client = make_ssh_client();
    EXPECT_CALL(mock_libssh, ssh_channel_get_exit_state).WillOnce(Return(SSH_ERROR));

    EXPECT_THROW(client->exec({{"foo"}}), std::runtime_error);
}

TEST_F(SSHClient, logSignalTerminationFromExitState)
{
    constexpr int failure_code{128 + 2}; // SIGINT exit status
    auto client = make_ssh_client();
    char signal[] = "INT";
    expect_exit_state(failure_code, signal);

    EXPECT_CALL(mock_libssh, ssh_string_free_char(signal)).Times(1);

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::error, "Process terminated by signal: INT");

    EXPECT_EQ(client->exec({{"foo"}}), static_cast<mp::VMReturnCode>(failure_code));
}

TEST_F(SSHClient, clientCanBeCreatedAgainAfterConstructionThrows)
{
    EXPECT_CALL(mock_libssh, ssh_channel_open_session(fake_channel))
        .WillOnce(Return(SSH_ERROR))
        .WillOnce(Return(SSH_OK));
    EXPECT_THROW(make_ssh_client(), std::runtime_error);

    EXPECT_NO_THROW(make_ssh_client());
}

#ifndef _WIN32
namespace
{
struct SSHClientLiveTerminal : public SSHClient
{
    SSHClientLiveTerminal()
    {
        master = posix_openpt(O_RDWR | O_NOCTTY);
        EXPECT_GE(master, 0);
        EXPECT_EQ(grantpt(master), 0);
        EXPECT_EQ(unlockpt(master), 0);
        slave = open(ptsname(master), O_RDWR | O_NOCTTY);
        EXPECT_GE(slave, 0);

        winsize size{24, 80, 0, 0};
        EXPECT_EQ(ioctl(slave, TIOCSWINSZ, &size), 0);

        ON_CALL(mock_term, cin_is_live).WillByDefault(Return(true));
        ON_CALL(mock_term, cout_is_live).WillByDefault(Return(true));
        ON_CALL(mock_term, native_handles)
            .WillByDefault(Return(mp::NativeHandles{.cin = slave, .cout = slave, .cerr = slave}));
    }

    ~SSHClientLiveTerminal()
    {
        close(slave);
        close(master);
    }

    termios slave_attrs() const
    {
        termios attrs{};
        EXPECT_EQ(tcgetattr(slave, &attrs), 0);
        return attrs;
    }

    int master = -1;
    int slave = -1;
    NiceMock<mpt::MockTerminal> mock_term;
};
} // namespace

TEST_F(SSHClientLiveTerminal, requestsPtyWithLocalTerminalSize)
{
    EXPECT_CALL(mock_libssh, ssh_channel_request_pty_size(fake_channel, _, 80, 24))
        .WillOnce(Return(SSH_OK));

    mp::PlatformSSHClient client{make_ssh_session(), &mock_term};
}

TEST_F(SSHClientLiveTerminal, putsTerminalInRawModeAndRestoresItOnDestruction)
{
    const auto original = slave_attrs();
    ASSERT_TRUE(original.c_lflag & ICANON);

    {
        mp::PlatformSSHClient client{make_ssh_session(), &mock_term};
        EXPECT_FALSE(slave_attrs().c_lflag & (ICANON | ECHO));
    }

    const auto restored = slave_attrs();
    EXPECT_EQ(restored.c_lflag, original.c_lflag);
    EXPECT_EQ(restored.c_iflag, original.c_iflag);
    EXPECT_EQ(restored.c_oflag, original.c_oflag);
}

TEST_F(SSHClientLiveTerminal, nonLiveTerminalSkipsPtyRequestAndRawMode)
{
    ON_CALL(mock_term, cin_is_live).WillByDefault(Return(false));
    const auto original = slave_attrs();

    EXPECT_CALL(mock_libssh, ssh_channel_request_pty_size).Times(0);

    mp::PlatformSSHClient client{make_ssh_session(), &mock_term};

    EXPECT_EQ(slave_attrs().c_lflag, original.c_lflag);
}
#endif

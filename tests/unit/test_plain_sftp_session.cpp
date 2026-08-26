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
#include "mock_logger.h"
#include "mock_sftp_client_steward.h"
#include "stub_ssh_key_provider.h"

#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
#include <multipass/format.h>
#include <multipass/ssh/plain_sftp_session.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/sshfs_mount/sftp_message.h>
#include <multipass/sshfs_mount/sftp_session.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
static_assert(!std::is_default_constructible_v<mp::SftpSession>, "only for derived classes");
static_assert(std::has_virtual_destructor_v<mp::SftpSession>);
static_assert(!std::is_copy_constructible_v<mp::SftpSession>);
static_assert(!std::is_copy_assignable_v<mp::SftpSession>);
static_assert(!std::is_copy_constructible_v<mp::PlainSftpSession>);
static_assert(!std::is_copy_assignable_v<mp::PlainSftpSession>);
static_assert(!std::is_move_constructible_v<mp::PlainSftpSession>);
static_assert(!std::is_move_assignable_v<mp::PlainSftpSession>);

// make_sftp_session consumes SSHSession
using MakeSftpSession = decltype(&mp::SSHSession::make_sftp_session);
static_assert(!std::is_invocable_v<MakeSftpSession,
                                   mp::SSHSession&,
                                   const mp::SftpClientSteward&,
                                   std::string,
                                   std::string>,
              "make_sftp_session must consume the session (callable only on an rvalue)");
static_assert(std::is_invocable_v<MakeSftpSession,
                                  mp::SSHSession&&,
                                  const mp::SftpClientSteward&,
                                  std::string,
                                  std::string>);

struct TestPlainSftpSession : public Test
{
    TestPlainSftpSession()
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning);

        ON_CALL(mock_libssh, ssh_new()).WillByDefault(Return(fake_session));
        ON_CALL(mock_libssh, ssh_options_set).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_connect).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_userauth_publickey).WillByDefault(Return(SSH_AUTH_SUCCESS));
        ON_CALL(mock_libssh, ssh_is_connected).WillByDefault(Return(1));
        ON_CALL(mock_libssh, ssh_channel_new).WillByDefault(Return(fake_channel));
        ON_CALL(mock_libssh, ssh_channel_open_session).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_get_fd).WillByDefault(Return(-1)); // no socket to shutdown
        ON_CALL(mock_libssh, ssh_get_error).WillByDefault(Return("mocked error"));
        ON_CALL(mock_libssh, ssh_remove_channel_callbacks).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_event_new()).WillByDefault(Return(fake_event));
        ON_CALL(mock_libssh, ssh_event_add_session).WillByDefault(Return(SSH_OK));

        // Remember what is running, to serve the result registered for it (see `exec_results`)
        ON_CALL(mock_libssh, ssh_channel_request_exec)
            .WillByDefault(DoAll(SaveArg<1>(&running_cmd), Return(SSH_OK)));
        // Save callbacks, to deliver exit code
        ON_CALL(mock_libssh, ssh_add_channel_callbacks)
            .WillByDefault(DoAll(SaveArg<1>(&channel_cbs), Return(SSH_OK)));
        ON_CALL(mock_libssh, ssh_event_dopoll).WillByDefault([this](ssh_event, int) {
            if (const auto& exit_code = result_for(running_cmd).exit_code)
                channel_cbs->channel_exit_status_function(fake_session,
                                                          fake_channel,
                                                          *exit_code,
                                                          channel_cbs->userdata);
            return SSH_OK;
        });

        // Serve the running command's output, until exhausted
        ON_CALL(mock_libssh, ssh_channel_read_timeout)
            .WillByDefault([this](ssh_channel, void* dest, uint32_t count, int is_stderr, int) {
                const auto& result = result_for(running_cmd);
                const auto& stream = is_stderr ? result.std_err : result.std_out;
                auto& read_so_far = bytes_read[{running_cmd, is_stderr != 0}];

                // num_bytes will be zero once the stream is exhausted
                const auto num_bytes = std::min<std::size_t>(count, stream.size() - read_so_far);

                std::memcpy(dest, stream.data() + read_so_far, num_bytes);
                read_so_far += num_bytes;

                return static_cast<int>(num_bytes); // a zero return signals successful completion
            });

        ON_CALL(client_steward, compose_client_command).WillByDefault(Return(client_cmd));

        init_fakes();
    }

    void init_fakes()
    {
        for (auto& fake_sftp_session : fake_sftp_sessions)
            fake_sftp_session.channel = fake_channel;
        for (auto& fake_client_msg : fake_client_msgs)
            fake_client_msg.type = SSH_FXP_INIT;
    }

    mp::PlainSSHSession make_ssh_session() const
    {
        return mp::PlainSSHSession{"host", 42, "ubuntu", key_provider};
    }

    std::unique_ptr<mp::SftpSession> make_sftp_session(
        std::optional<mp::PlainSSHSession> ssh_session = std::nullopt) const
    {
        return (ssh_session ? std::move(*ssh_session) : make_ssh_session())
            .make_sftp_session(client_steward, source, target);
    }

    struct ExecResult
    {
        std::optional<int> exit_code = std::nullopt;
        std::string std_out = {};
        std::string std_err = {};
    };

    const ExecResult& result_for(const std::string& cmd) const
    {
        static const auto default_result = ExecResult{};
        const auto it = exec_results.find(cmd);
        return it == exec_results.end() ? default_result : it->second;
    }

    template <size_t times>
    void expect_client_spawns()
    {
        static_assert(times <= max_fakes, "not enough fakes to go around");

        // Whatever else the session may run is up to individual tests to care about
        EXPECT_CALL(mock_libssh, ssh_channel_request_exec).Times(AnyNumber());
        EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq(client_cmd)))
            .Times(times);

        auto& server_new = EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel));
        for (std::size_t i = 0; i < times; ++i)
        {
            server_new.WillOnce(Return(&fake_sftp_sessions[i]));
            EXPECT_CALL(mock_libssh, sftp_get_client_message(&fake_sftp_sessions[i]))
                .WillOnce(Return(&fake_client_msgs[i]));
            EXPECT_CALL(mock_libssh, sftp_server_free(&fake_sftp_sessions[i])).Times(1);
        }

        EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0))
            .Times(times)
            .WillRepeatedly(Return(1));
        EXPECT_CALL(mock_libssh, sftp_reply_version(_)).Times(times).WillRepeatedly(Return(SSH_OK));
    }

    constexpr static auto source = "/host/source";
    constexpr static auto target = "/guest/target";
    static inline const std::string client_cmd = fmt::format("sudo -n sshfs -o slave :{} {}",
                                                             source,
                                                             target);

    // Fakes for the SFTP sessions that successive clients get served, and their init messages
    constexpr static size_t max_fakes = 3;
    std::array<sftp_session_struct, max_fakes> fake_sftp_sessions{};
    std::array<sftp_client_message_struct, max_fakes> fake_client_msgs{};

    std::string running_cmd;                                        ///< currently executing cmd
    std::map<std::string, ExecResult> exec_results;                 ///< results, by command
    std::map<std::pair<std::string, bool>, std::size_t> bytes_read; ///< by command and stream

    NiceMock<mpt::MockSftpClientSteward> client_steward;
    mpt::StubSSHKeyProvider key_provider;
    mpt::MockLibssh::GuardedMock guarded_mock = mpt::MockLibssh::inject<NiceMock>();
    mpt::MockLibssh& mock_libssh = *guarded_mock.first;
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    constexpr static auto bad_addr = 0xdeadbeefdeadbeefull; // should reliably segfault on 32/64-bit
    ssh_session fake_session = reinterpret_cast<ssh_session>(bad_addr);
    ssh_channel fake_channel = reinterpret_cast<ssh_channel>(bad_addr);
    ssh_event fake_event = reinterpret_cast<ssh_event>(bad_addr);

    ssh_channel_callbacks channel_cbs = nullptr;
};
} // namespace

TEST_F(TestPlainSftpSession, makeSftpSessionRunsDerivedClientCommand)
{
    exec_results[client_cmd] = {.exit_code = 1};

    auto session = make_ssh_session();
    EXPECT_CALL(client_steward, compose_client_command(_, StrEq(source), StrEq(target)))
        .WillOnce(Return(client_cmd));
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq(client_cmd)));

    EXPECT_ANY_THROW(static_cast<void>(make_sftp_session(std::move(session))));
}

TEST_F(TestPlainSftpSession, makeSftpSessionThrowsWhenClientFails)
{
    const std::string error = "sshfs bonkers";
    exec_results[client_cmd] = {.exit_code = 127, .std_err = error};

    auto session = make_ssh_session();

    MP_EXPECT_THROW_THAT(static_cast<void>(make_sftp_session(std::move(session))),
                         std::runtime_error,
                         mpt::match_what(StrEq(error)));
}

TEST_F(TestPlainSftpSession, releasesConsumedSessionOnce)
{
    exec_results[client_cmd] = {.exit_code = 127};

    auto session = make_ssh_session();
    {
        EXPECT_CALL(mock_libssh, ssh_channel_free(fake_channel)).Times(1);
        EXPECT_CALL(mock_libssh, ssh_free(fake_session)).Times(1);

        EXPECT_ANY_THROW(static_cast<void>(make_sftp_session(std::move(session))));
    } // session internals freed

    EXPECT_TRUE(session.is_moved());
}

TEST_F(TestPlainSftpSession, makeSftpSessionThrowsWhenServerCreationFails)
{
    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel)).WillOnce(Return(nullptr));

    auto session = make_ssh_session();
    MP_EXPECT_THROW_THAT(static_cast<void>(make_sftp_session(std::move(session))),
                         mp::SSHException,
                         mpt::match_what(HasSubstr("could not create a new sftp_server")));
}

TEST_F(TestPlainSftpSession, makeSftpSessionThrowsWhenSessionTimeoutCannotBeSet)
{
    // created first, so its own (coincidentally identical) SSH_OPTIONS_TIMEOUT calls are consumed
    // under the default before the EXPECT_CALL below targets the one inside make_raw_sftp_session
    auto session = make_ssh_session();

    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel))
        .WillOnce(Return(&fake_sftp_sessions[0]));
    EXPECT_CALL(mock_libssh, ssh_options_set(_, Eq(SSH_OPTIONS_TIMEOUT), _))
        .WillOnce(Return(SSH_ERROR));
    EXPECT_CALL(mock_libssh, sftp_server_free(&fake_sftp_sessions[0])).Times(1);

    MP_EXPECT_THROW_THAT(static_cast<void>(make_sftp_session(std::move(session))),
                         mp::SSHException,
                         mpt::match_what(HasSubstr("could not set session timeout")));
}

TEST_F(TestPlainSftpSession, makeSftpSessionThrowsOnHandshakeConnectionDrop)
{
    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel))
        .WillOnce(Return(&fake_sftp_sessions[0]));
    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0))
        .WillOnce(Return(SSH_ERROR));
    EXPECT_CALL(mock_libssh, sftp_server_free(&fake_sftp_sessions[0])).Times(1);

    auto session = make_ssh_session();
    MP_EXPECT_THROW_THAT(static_cast<void>(make_sftp_session(std::move(session))),
                         mp::SSHException,
                         mpt::match_what(HasSubstr("connection drop or malfunction")));
}

TEST_F(TestPlainSftpSession, makeSftpSessionThrowsOnHandshakeTimeout)
{
    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel))
        .WillOnce(Return(&fake_sftp_sessions[0]));
    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0)).WillOnce(Return(0));
    EXPECT_CALL(mock_libssh, sftp_server_free(&fake_sftp_sessions[0])).Times(1);

    auto session = make_ssh_session();
    MP_EXPECT_THROW_THAT(
        static_cast<void>(make_sftp_session(std::move(session))),
        mp::SSHException,
        mpt::match_what(HasSubstr("timed out waiting for the initial client message")));
}

TEST_F(TestPlainSftpSession, makeSftpSessionSucceeds)
{
    auto session = make_ssh_session();
    ASSERT_FALSE(session.is_moved());

    expect_client_spawns<1>();

    EXPECT_THAT(make_sftp_session(std::move(session)), NotNull());
}

TEST_F(TestPlainSftpSession, renewClientCleansUpAfterFormerClientAndRespawns)
{
    expect_client_spawns<2>(); // one client to begin with, another one to replace it
    EXPECT_CALL(client_steward, clean_up_after_client(_, StrEq(source))).Times(2);

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->renew_client();
}

TEST_F(TestPlainSftpSession, renewClientSupportsMultipleRespawns)
{
    expect_client_spawns<3>(); // one client to begin with, two more respawns
    EXPECT_CALL(client_steward, clean_up_after_client(_, StrEq(source))).Times(3);

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->renew_client();
    sftp_session->renew_client();
}

TEST_F(TestPlainSftpSession, renewClientNoopAfterStopRequested)
{
    expect_client_spawns<1>(); // only the original client, no replacement
    EXPECT_CALL(client_steward, clean_up_after_client).Times(1);

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->request_stop();
    sftp_session->renew_client();

    // the pending stop request is not forgotten either
    EXPECT_THAT(sftp_session->next_message(), IsNull());
}

TEST_F(TestPlainSftpSession, renewClientPropagatesCleanUpFailure)
{
    const std::string error = "could not unmount";

    expect_client_spawns<1>(); // the replacement never gets to run
    EXPECT_CALL(client_steward, clean_up_after_client)
        .WillOnce(Throw(std::runtime_error{error}))
        .WillOnce(Return());

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    MP_EXPECT_THROW_THAT(sftp_session->renew_client(),
                         std::runtime_error,
                         mpt::match_what(StrEq(error)));
}

TEST_F(TestPlainSftpSession, renewClientPropagatesRespawnFailure)
{
    expect_client_spawns<1>(); // the second spawn attempt will fail
    EXPECT_CALL(client_steward, clean_up_after_client).Times(2); // once here, once at destruction

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq(client_cmd))).Times(1);
    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_libssh, ssh_channel_free(fake_channel)).Times(2); // the 2nd one is still freed

    MP_EXPECT_THROW_THAT(sftp_session->renew_client(),
                         mp::SSHException,
                         mpt::match_what(HasSubstr("could not create a new sftp_server")));
}

TEST_F(TestPlainSftpSession, nextMessageReturnsMessageWhenPollSucceeds)
{
    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0)).WillOnce(Return(1));
    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fake_sftp_sessions[0]))
        .WillOnce(Return(&fake_client_msgs[1]));

    EXPECT_THAT(sftp_session->next_message(), NotNull());
}

TEST_F(TestPlainSftpSession, nextMessagePollsAgainWhenNothingToReadYet)
{
    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0))
        .WillOnce(Return(0))
        .WillOnce(Return(1));
    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fake_sftp_sessions[0]))
        .WillOnce(Return(&fake_client_msgs[1]));

    EXPECT_THAT(sftp_session->next_message(), NotNull());
}

TEST_F(TestPlainSftpSession, nextMessageReturnsNullOnError)
{
    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0))
        .WillOnce(Return(SSH_ERROR))
        .WillOnce(Return(1));
    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fake_sftp_sessions[0]))
        .WillOnce(Return(&fake_client_msgs[1]));

    EXPECT_THAT(sftp_session->next_message(), IsNull());

    // a further call still polls normally
    EXPECT_THAT(sftp_session->next_message(), NotNull());
}

TEST_F(TestPlainSftpSession, nextMessageReturnsNullWhenGetClientMessageFails)
{
    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0))
        .Times(2)
        .WillRepeatedly(Return(1));
    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fake_sftp_sessions[0]))
        .WillOnce(Return(nullptr)) // poll said data's ready, but the read itself failed
        .WillOnce(Return(&fake_client_msgs[1]));

    EXPECT_THAT(sftp_session->next_message(), IsNull());

    // a further call still polls normally
    EXPECT_THAT(sftp_session->next_message(), NotNull());
}

TEST_F(TestPlainSftpSession, clientExitCodeReportsGracefulClientExit)
{
    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    // only now: a client that had already exited would keep the session from starting
    exec_results[client_cmd] = {.exit_code = 0};

    EXPECT_THAT(sftp_session->client_exit_code(), Optional(0));
}

TEST_F(TestPlainSftpSession, clientExitCodeReportsFailedClientExit)
{
    constexpr auto failure_code = 127;

    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    exec_results[client_cmd] = {.exit_code = failure_code};

    EXPECT_THAT(sftp_session->client_exit_code(), Optional(failure_code));
}

TEST_F(TestPlainSftpSession, clientExitCodeEmptyWhenClientDoesNotReport)
{
    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    // no exit code is registered for the client, so it never reports one
    EXPECT_THAT(sftp_session->client_exit_code(), Eq(std::nullopt));
}

TEST_F(TestPlainSftpSession, clientExitCodePropagatesSSHError)
{
    expect_client_spawns<1>();

    auto sftp_session = make_sftp_session();
    ASSERT_THAT(sftp_session, NotNull());

    ON_CALL(mock_libssh, ssh_event_dopoll).WillByDefault(Return(SSH_ERROR));

    MP_EXPECT_THROW_THAT(sftp_session->client_exit_code(),
                         mp::SSHProcessExitError,
                         mpt::match_what(HasSubstr("ssh_event_dopoll failed")));
}

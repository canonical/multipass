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
#include "mock_sftp_client_composer.h"
#include "stub_ssh_key_provider.h"

#include <multipass/ssh/plain_sftp_session.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/sshfs_mount/sftp_session.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <utility>

namespace mp = multipass;
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
                                   const mp::SftpClientComposer&,
                                   std::string,
                                   std::string>,
              "make_sftp_session must consume the session (callable only on an rvalue)");
static_assert(std::is_invocable_v<MakeSftpSession,
                                  mp::SSHSession&&,
                                  const mp::SftpClientComposer&,
                                  std::string,
                                  std::string>);

struct TestPlainSftpSession : public Test
{
    TestPlainSftpSession()
    {
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
            channel_cbs->channel_exit_status_function(fake_session,
                                                      fake_channel,
                                                      result_for(running_cmd).exit_code,
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

        ON_CALL(client_composer, compose_client_command).WillByDefault(Return(client_cmd));
    }

    mp::PlainSSHSession make_ssh_session() const
    {
        return mp::PlainSSHSession{"host", 42, "ubuntu", key_provider};
    }

    std::unique_ptr<mp::SftpSession> make_sftp_session(mp::PlainSSHSession&& ssh_session) const
    {
        return std::move(ssh_session).make_sftp_session(client_composer, source, target);
    }

    struct ExecResult
    {
        int exit_code = 0;
        std::string std_out = {};
        std::string std_err = {};
    };

    const ExecResult& result_for(const std::string& cmd) const
    {
        static const auto default_result = ExecResult{};
        const auto it = exec_results.find(cmd);
        return it == exec_results.end() ? default_result : it->second;
    }

    std::map<std::string, ExecResult> exec_results;                 ///< results, by command
    std::string running_cmd;                                        ///< currently executing cmd
    std::map<std::pair<std::string, bool>, std::size_t> bytes_read; ///< by command and stream

    constexpr static auto source = "/host/source";
    constexpr static auto target = "/guest/target";
    static inline const std::string client_cmd = fmt::format("sudo -n sshfs -o slave :{} {}",
                                                             source,
                                                             target);

    NiceMock<mpt::MockSftpClientComposer> client_composer;
    mpt::StubSSHKeyProvider key_provider;
    mpt::MockLibssh::GuardedMock guarded_mock = mpt::MockLibssh::inject<NiceMock>();
    mpt::MockLibssh& mock_libssh = *guarded_mock.first;

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
    EXPECT_CALL(client_composer, compose_client_command(_, StrEq(source), StrEq(target)))
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

TEST_F(TestPlainSftpSession, makeSftpSessionSucceeds)
{
    auto session = make_ssh_session();
    ASSERT_FALSE(session.is_moved());

    sftp_session_struct fake_sftp_session{};
    fake_sftp_session.channel = fake_channel;
    sftp_client_message_struct fake_client_msg{};
    fake_client_msg.type = SSH_FXP_INIT;

    // Don't invoke the exit-status callback so the client doesn't appear to have exited
    EXPECT_CALL(mock_libssh, ssh_event_dopoll(fake_event, _)).WillRepeatedly(Return(SSH_OK));

    // borrow_session/borrow_channel are private-pass-gated and called nowhere but here (the ctor
    // below), so this expectation on their sftp_server_new args is the only way to check them.
    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel))
        .WillOnce(Return(&fake_sftp_session));
    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0)).WillOnce(Return(1));
    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fake_sftp_session))
        .WillOnce(Return(&fake_client_msg));
    EXPECT_CALL(mock_libssh, sftp_reply_version(&fake_client_msg)).WillOnce(Return(SSH_OK));

    EXPECT_THAT(make_sftp_session(std::move(session)), NotNull());
}

TEST_F(TestPlainSftpSession, renewClientRespawnsClient)
{
    sftp_session_struct fake_sftp_session{};
    fake_sftp_session.channel = fake_channel;
    sftp_client_message_struct fake_client_msg{};
    fake_client_msg.type = SSH_FXP_INIT;

    using FakePair = std::pair<sftp_session_struct, sftp_client_message_struct>;
    FakePair fake_pair = {fake_sftp_session, fake_client_msg};

    using Fakes = std::array<FakePair, 2>;
    Fakes fakes{};
    fakes.fill(fake_pair);

    // Don't invoke the exit-status callback so the client doesn't appear to have exited
    EXPECT_CALL(mock_libssh, ssh_event_dopoll(fake_event, _)).WillRepeatedly(Return(SSH_OK));

    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq(client_cmd))).Times(2);
    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel))
        .WillOnce(Return(&fakes[0].first))
        .WillOnce(Return(&fakes[1].first));

    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0))
        .Times(2)
        .WillRepeatedly(Return(1));

    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fakes[0].first))
        .WillOnce(Return(&fakes[0].second));
    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fakes[1].first))
        .WillOnce(Return(&fakes[1].second));
    EXPECT_CALL(mock_libssh, sftp_reply_version(_)).Times(2).WillRepeatedly(Return(SSH_OK));

    auto sftp_session = make_sftp_session(make_ssh_session());

    {
        EXPECT_CALL(mock_libssh, sftp_server_free(&fakes[0].first)).Times(1);

        ASSERT_THAT(sftp_session, NotNull());

        sftp_session->renew_client();
    }

    EXPECT_CALL(mock_libssh, sftp_server_free(&fakes[1].first)).Times(1);
}

TEST_F(TestPlainSftpSession, renewClientNoopAfterStopRequested)
{
    sftp_session_struct fake_sftp_session{};
    fake_sftp_session.channel = fake_channel;
    sftp_client_message_struct fake_client_msg{};
    fake_client_msg.type = SSH_FXP_INIT;

    // Don't invoke the exit-status callback so the client doesn't appear to have exited
    EXPECT_CALL(mock_libssh, ssh_event_dopoll(fake_event, _)).WillRepeatedly(Return(SSH_OK));

    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq(client_cmd))).Times(1);
    EXPECT_CALL(mock_libssh, sftp_server_new(fake_session, fake_channel))
        .WillOnce(Return(&fake_sftp_session));

    EXPECT_CALL(mock_libssh, ssh_channel_poll_timeout(fake_channel, _, 0)).WillOnce(Return(1));
    EXPECT_CALL(mock_libssh, sftp_get_client_message(&fake_sftp_session))
        .WillOnce(Return(&fake_client_msg));
    EXPECT_CALL(mock_libssh, sftp_reply_version(&fake_client_msg)).WillOnce(Return(SSH_OK));
    EXPECT_CALL(mock_libssh, sftp_server_free(&fake_sftp_session)).Times(1);

    auto sftp_session = make_sftp_session(make_ssh_session());
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->request_stop();
    sftp_session->renew_client();
}

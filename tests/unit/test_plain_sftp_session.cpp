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

        ON_CALL(client_composer, compose_client_command).WillByDefault(Return(client_cmd));

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

    std::unique_ptr<mp::SftpSession> make_sftp_session(mp::PlainSSHSession&& ssh_session) const
    {
        return std::move(ssh_session).make_sftp_session(client_composer, source, target);
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
    static inline const auto findmnt_cmd = fmt::format("findmnt --source :{} -o TARGET -n", source);

    // Fakes for the SFTP sessions that successive clients get served, and their init messages
    constexpr static size_t max_fakes = 2;
    std::array<sftp_session_struct, max_fakes> fake_sftp_sessions{};
    std::array<sftp_client_message_struct, max_fakes> fake_client_msgs{};

    std::string running_cmd;                                        ///< currently executing cmd
    std::map<std::string, ExecResult> exec_results;                 ///< results, by command
    std::map<std::pair<std::string, bool>, std::size_t> bytes_read; ///< by command and stream

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

    expect_client_spawns<1>();

    EXPECT_THAT(make_sftp_session(std::move(session)), NotNull());
}

TEST_F(TestPlainSftpSession, renewClientRespawnsClient)
{
    expect_client_spawns<2>(); // one client to begin with, another one to replace it

    auto sftp_session = make_sftp_session(make_ssh_session());
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->renew_client();
}

TEST_F(TestPlainSftpSession, renewClientNoopAfterStopRequested)
{
    expect_client_spawns<1>(); // only the original client, no replacement

    auto sftp_session = make_sftp_session(make_ssh_session());
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->request_stop();
    sftp_session->renew_client();

    // the pending stop request is not forgotten either
    EXPECT_THAT(sftp_session->next_message(), IsNull());
}

TEST_F(TestPlainSftpSession, renewClientUnmountsStaleMountBeforeRespawning)
{
    constexpr auto mount_path = "/guest/target";
    const auto umount_cmd = fmt::format("sudo umount {}", mount_path);
    exec_results[findmnt_cmd] = {.std_out = fmt::format("{}\n", mount_path)};
    exec_results[umount_cmd] = {.exit_code = 0};

    expect_client_spawns<2>();
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq(umount_cmd)));

    auto sftp_session = make_sftp_session(make_ssh_session());
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->renew_client();
}

TEST_F(TestPlainSftpSession, renewClientSkipsUnmountWhenNothingMounted)
{
    // No mount to be found: findmnt says so with an empty answer and a non-zero exit
    exec_results[findmnt_cmd] = {.exit_code = 1};

    expect_client_spawns<2>();
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(_, HasSubstr("umount"))).Times(0);

    auto sftp_session = make_sftp_session(make_ssh_session());
    ASSERT_THAT(sftp_session, NotNull());

    sftp_session->renew_client();
}

TEST_F(TestPlainSftpSession, renewClientThrowsWhenUnmountFails)
{
    constexpr auto mount_path = "/guest/target";
    exec_results[findmnt_cmd] = {.std_out = fmt::format("{}\n", mount_path)};
    exec_results[fmt::format("sudo umount {}", mount_path)] = {.exit_code = 1,
                                                               .std_err = "not mounted"};

    expect_client_spawns<1>(); // the replacement never gets to run

    auto sftp_session = make_sftp_session(make_ssh_session());
    ASSERT_THAT(sftp_session, NotNull());

    MP_EXPECT_THROW_THAT(sftp_session->renew_client(),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("not mounted")));
}

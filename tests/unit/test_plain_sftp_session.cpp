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
#include <cstring>
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
        ON_CALL(mock_libssh, ssh_channel_request_exec).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_get_fd).WillByDefault(Return(-1)); // no socket to shutdown
        ON_CALL(mock_libssh, ssh_get_error).WillByDefault(Return("mocked error"));

        // Exit-status machinery: deliver `sshfs_exit_code` through the registered callback when
        // the event loop polls, as libssh would on a channel-exit-status message
        ON_CALL(mock_libssh, ssh_add_channel_callbacks)
            .WillByDefault(DoAll(SaveArg<1>(&channel_cbs), Return(SSH_OK)));
        ON_CALL(mock_libssh, ssh_remove_channel_callbacks).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_event_new()).WillByDefault(Return(fake_event));
        ON_CALL(mock_libssh, ssh_event_add_session).WillByDefault(Return(SSH_OK));
        ON_CALL(mock_libssh, ssh_event_dopoll).WillByDefault([this](ssh_event, int) {
            channel_cbs->channel_exit_status_function(fake_session,
                                                      fake_channel,
                                                      sshfs_exit_code,
                                                      channel_cbs->userdata);
            return SSH_OK;
        });

        ON_CALL(client_composer, compose_client_command).WillByDefault(Return(sshfs_cmd));
    }

    mp::PlainSSHSession make_ssh_session() const
    {
        return mp::PlainSSHSession{"host", 42, "ubuntu", key_provider};
    }

    std::unique_ptr<mp::SftpSession> make_sftp_session(mp::PlainSSHSession&& ssh_session) const
    {
        return std::move(ssh_session).make_sftp_session(client_composer, source, target);
    }

    constexpr static auto source = "/host/source";
    constexpr static auto target = "/guest/target";
    static inline const std::string sshfs_cmd = fmt::format("sudo -n sshfs -o slave :{} {}",
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
    int sshfs_exit_code = 0;
};
} // namespace

TEST_F(TestPlainSftpSession, makeSftpSessionRunsDerivedClientCommand)
{
    sshfs_exit_code = 1; // TODO@sftp mock success path instead

    auto session = make_ssh_session();
    EXPECT_CALL(client_composer, compose_client_command(_, StrEq(source), StrEq(target)))
        .WillOnce(Return(sshfs_cmd));
    EXPECT_CALL(mock_libssh, ssh_channel_request_exec(fake_channel, StrEq(sshfs_cmd)))
        .WillOnce(Return(SSH_OK));

    EXPECT_ANY_THROW(static_cast<void>(make_sftp_session(std::move(session))));
}

TEST_F(TestPlainSftpSession, makeSftpSessionThrowsWhenClientFails)
{
    sshfs_exit_code = 127;
    const std::string error = "sshfs bonkers";
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout).WillRepeatedly(Return(0));
    EXPECT_CALL(mock_libssh, ssh_channel_read_timeout(_, _, _, Ne(0), _))
        .WillOnce(WithArgs<1, 2>([&error](void* dest, uint32_t count) {
            const auto num_bytes = std::min<std::size_t>(error.size(), count);
            std::memcpy(dest, error.data(), num_bytes);
            return static_cast<int>(num_bytes);
        }))
        .RetiresOnSaturation();

    auto session = make_ssh_session();

    MP_EXPECT_THROW_THAT(static_cast<void>(make_sftp_session(std::move(session))),
                         std::runtime_error,
                         mpt::match_what(StrEq(error)));
}

TEST_F(TestPlainSftpSession, releasesConsumedSessionOnce)
{
    sshfs_exit_code = 127;

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

    // Don't invoke the exit-status callback so sshfs doesn't appear to have exited
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

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

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/plain_ssh_session.h>

#include <type_traits>
#include <utility>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
static_assert(std::is_final_v<mp::PlainSSHSession>, "required to prevent chopping on move");
static_assert(!std::is_copy_constructible_v<mp::PlainSSHSession>);
static_assert(!std::is_copy_assignable_v<mp::PlainSSHSession>);
static_assert(std::is_move_constructible_v<mp::PlainSSHSession>);
static_assert(std::is_move_assignable_v<mp::PlainSSHSession>);
static_assert(!std::is_copy_constructible_v<mp::SSHSession>);
static_assert(!std::is_copy_assignable_v<mp::SSHSession>);

struct TestPlainSSHSessionMockedLibssh : public Test
{
    TestPlainSSHSessionMockedLibssh()
    {
        ON_CALL(libssh, ssh_new()).WillByDefault(Return(fake_session));
        ON_CALL(libssh, ssh_options_set).WillByDefault(Return(SSH_OK));
        ON_CALL(libssh, ssh_connect).WillByDefault(Return(SSH_OK));
        ON_CALL(libssh, ssh_userauth_publickey).WillByDefault(Return(SSH_AUTH_SUCCESS));
        ON_CALL(libssh, ssh_is_connected).WillByDefault(Return(1));
        ON_CALL(libssh, ssh_channel_new).WillByDefault(Return(fake_channel));
        ON_CALL(libssh, ssh_channel_open_session).WillByDefault(Return(SSH_OK));
        ON_CALL(libssh, ssh_get_fd).WillByDefault(Return(-1)); // no socket to shutdown
    }

    mp::PlainSSHSession make_ssh_session() const
    {
        return mp::PlainSSHSession{"host", 42, "ubuntu", key_provider};
    }

    mpt::MockLibssh::GuardedMock guarded_mock = mpt::MockLibssh::inject<NiceMock>();
    mpt::MockLibssh& libssh = *guarded_mock.first;

    constexpr static auto bad_addr = 0xdeadbeefdeadbeefull; // should reliably segfault on 32/64-bit
    ssh_session fake_session = reinterpret_cast<ssh_session>(bad_addr);
    ssh_channel fake_channel = reinterpret_cast<ssh_channel>(bad_addr);

    mpt::StubSSHKeyProvider key_provider;
};
} // namespace

TEST_F(TestPlainSSHSessionMockedLibssh, execPlainReturnsConcreteProcessRunningGivenCommand)
{
    auto session = make_ssh_session();
    EXPECT_CALL(libssh, ssh_channel_request_exec(fake_channel, StrEq("ls -la")))
        .WillOnce(Return(SSH_OK));

    std::unique_ptr<mp::PlainSSHProcess> proc = session.exec_plain("ls -la");

    ASSERT_THAT(proc, NotNull());
    EXPECT_EQ(proc->get_cmd(), "ls -la");
}

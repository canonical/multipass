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

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/ssh/plain_ssh_process.h>

#include <mutex>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TestPlainSSHProcess : public Test
{
    mp::PlainSSHProcess make_ssh_process(const std::string& cmd = "cmd")
    {
        return mp::PlainSSHProcess{*fake_session, cmd, std::unique_lock{mutex}};
    }

    mpt::MockLibssh::GuardedMock guarded_mock = mpt::MockLibssh::inject<NiceMock>();
    mpt::MockLibssh& mock_libssh = *guarded_mock.first;

    constexpr static auto bad_addr = 0xdeadbeefdeadbeefull; // should reliably segfault on 32/64-bit
    ssh_session fake_session = reinterpret_cast<ssh_session>(bad_addr);
    ssh_channel fake_channel = reinterpret_cast<ssh_channel>(bad_addr);

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
    ON_CALL(mock_libssh, ssh_is_connected).WillByDefault(Return(1));
    ON_CALL(mock_libssh, ssh_channel_new).WillByDefault(Return(fake_channel));
    EXPECT_CALL(mock_libssh, ssh_channel_open_session).WillOnce(Return(SSH_ERROR));
    EXPECT_CALL(mock_libssh, ssh_get_error(fake_session)).WillOnce(Return(err));

    MP_EXPECT_THROW_THAT(make_ssh_process(), mp::SSHException, mpt::match_what(HasSubstr(err)));
}

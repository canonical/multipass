/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/ssh/ssh_session.h>

#include <gmock/gmock.h>

#include "dummy_ssh_key_provider.h"
#include "mock_ssh.h"
#include "stub_base_virtual_machine.h"

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct BaseVM : public Test
{
    const mpt::DummyKeyProvider key_provider{"keeper of the seven keys"};
};
} // namespace

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_opening_a_session)
{
    mpt::StubBaseVirtualMachine base_vm;

    REPLACE(ssh_new, []() { return nullptr; }); // This makes SSH throw when opening a new session.
    EXPECT_THROW(mp::SSHSession("theanswertoeverything", 42), mp::SSHException); // Test that it indeed does.

    auto ipv4_count = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ipv4_count.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_executing)
{
    mpt::StubBaseVirtualMachine base_vm;

    // Make SSH throw when trying to execute something.
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    REPLACE(ssh_is_connected, [](auto...) { return true; });
    REPLACE(ssh_channel_open_session, [](auto...) { return SSH_OK; });
    REPLACE(ssh_channel_request_exec, [](auto...) { return SSH_ERROR; });
    REPLACE(ssh_userauth_publickey, [](auto...) { return SSH_OK; });

    // Check that it indeed throws at execution.
    mp::SSHSession session{"host", 42};
    EXPECT_THROW(session.exec("dummy"), mp::SSHException);

    auto ipv4_count = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ipv4_count.size(), 0u);
}

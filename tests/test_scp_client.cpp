/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "mock_scp.h"
#include "mock_ssh.h"

#include "file_operations.h"
#include "path.h"
#include "temp_dir.h"

#include <multipass/ssh/scp_client.h>
#include <multipass/ssh/ssh_session.h>

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
struct SCPClient : public testing::Test
{
    SCPClient()
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);
    }

    mp::SCPClient make_scp_client()
    {
        return {std::make_unique<mp::SSHSession>("a", 42)};
    }

    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};
};
}

TEST_F(SCPClient, throws_when_unable_to_allocate_scp_session)
{
    auto scp = make_scp_client();

    REPLACE(ssh_scp_new, [](auto...) { return nullptr; });

    EXPECT_THROW(scp.push_file("foo", "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_when_failed_to_init)
{
    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(scp.push_file("foo", "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_when_push_file_fails)
{
    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_push_file, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(scp.push_file("foo", "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_on_scp_write_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_push_file, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_write, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(scp.push_file(file_name.toStdString(), "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_on_push_file_scp_close_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_push_file, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_write, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_close, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(scp.push_file(file_name.toStdString(), "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_on_push_file_invalid_source)
{
    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_push_file, [](auto...) { return SSH_OK; });

    EXPECT_THROW(scp.push_file("/foo/bar", "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_when_pull_file_request_not_newfile)
{
    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_pull_request, [](auto...) { return SSH_SCP_REQUEST_WARNING; });

    EXPECT_THROW(scp.pull_file("foo", "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_when_accept_request_fails)
{
    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_pull_request, [](auto...) { return SSH_SCP_REQUEST_NEWFILE; });
    REPLACE(ssh_scp_request_get_size, [](auto...) { return 100; });
    REPLACE(ssh_scp_request_get_filename, [](auto...) { return "foo"; });
    REPLACE(ssh_scp_accept_request, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(scp.pull_file("foo", "bar"), std::runtime_error);
}

TEST_F(SCPClient, throws_on_pull_file_scp_close_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_pull_request, [](auto...) { return SSH_SCP_REQUEST_EOF; });
    REPLACE(ssh_scp_request_get_size, [](auto...) { return 100; });
    REPLACE(ssh_scp_request_get_filename, [](auto...) { return "foo"; });
    REPLACE(ssh_scp_accept_request, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_read, [](auto...) { return 0; });
    REPLACE(ssh_scp_close, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(scp.pull_file("foo", file_name.toStdString()), std::runtime_error);
}

TEST_F(SCPClient, throws_on_on_pull_file_invalid_destination)
{
    auto scp = make_scp_client();

    REPLACE(ssh_scp_init, [](auto...) { return SSH_OK; });
    REPLACE(ssh_scp_pull_request, [](auto...) { return SSH_SCP_REQUEST_NEWFILE; });
    REPLACE(ssh_scp_request_get_size, [](auto...) { return 100; });
    REPLACE(ssh_scp_request_get_filename, [](auto...) { return "foo"; });

    EXPECT_THROW(scp.pull_file("foo", "/foo/bar"), std::runtime_error);
}

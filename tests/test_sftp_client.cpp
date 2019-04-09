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

#include "mock_sftp.h"
#include "mock_ssh.h"

#include "file_operations.h"
#include "path.h"
#include "temp_dir.h"

#include <multipass/ssh/sftp_client.h>
#include <multipass/ssh/ssh_session.h>

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
struct SFTPClient : public testing::Test
{
    SFTPClient()
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);
    }

    mp::SFTPClient make_sftp_client()
    {
        return {std::make_unique<mp::SSHSession>("b", 43)};
    }

    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};
};
}

TEST_F(SFTPClient, throws_when_unable_to_allocate_scp_session)
{
    auto sftp = make_sftp_client();

    REPLACE(sftp_new, [](auto...) { return nullptr; });

    EXPECT_THROW(sftp.push_file("foo", "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_when_failed_to_init)
{
    auto sftp = make_sftp_client();

    REPLACE(sftp_init, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(sftp.push_file("foo", "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_on_sftp_write_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftp_client();

    REPLACE(sftp_open, [](auto...) { return new sftp_file_struct; });

    EXPECT_THROW(sftp.push_file(file_name.toStdString(), "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_on_push_file_sftp_close_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftp_client();

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](auto...) { return new sftp_file_struct;});
    REPLACE(sftp_write, [](auto...) { return 0; });
    REPLACE(sftp_close, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(sftp.push_file(file_name.toStdString(), "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_on_push_file_invalid_source)
{
    auto sftp = make_sftp_client();

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](auto...) { return new sftp_file_struct; });
    REPLACE(sftp_get_error, [](auto...) { return SSH_OK; });

    EXPECT_THROW(sftp.push_file("/foo/bar", "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_when_pull_file_error_getting_stat)
{
    auto sftp = make_sftp_client();

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });

    EXPECT_THROW(sftp.pull_file("foo", "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_on_pull_file_invalid_source)
{
    auto sftp = make_sftp_client();

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](auto...) { return new sftp_file_struct; });

    EXPECT_THROW(sftp.pull_file("foo", "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_on_sftp_read_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftp_client();

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return new sftp_attributes_struct; });
    REPLACE(sftp_open, [](auto...) { return new sftp_file_struct; });

    EXPECT_THROW(sftp.push_file(file_name.toStdString(), "bar"), std::runtime_error);
}

TEST_F(SFTPClient, throws_on_pull_file_scp_close_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    auto sftp = make_sftp_client();

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return new sftp_attributes_struct; });
    REPLACE(sftp_open, [](auto...) { return new sftp_file_struct; });
    REPLACE(sftp_read, [](auto...) { return 0; });
    REPLACE(sftp_close, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(sftp.pull_file("foo", file_name.toStdString()), std::runtime_error);
}

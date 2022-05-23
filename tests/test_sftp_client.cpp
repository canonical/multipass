/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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
#include "file_operations.h"
#include "mock_sftp.h"
#include "mock_ssh_test_fixture.h"
#include "path.h"
#include "temp_dir.h"

#include <multipass/ssh/sftp_client.h>
#include <multipass/ssh/ssh_session.h>

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
sftp_file get_dummy_sftp_file()
{
    return static_cast<sftp_file_struct*>(calloc(1, sizeof(struct sftp_file_struct)));
}

struct SFTPClient : public testing::Test
{
    SFTPClient()
        : sftp_new{mock_sftp_new,
                   [](ssh_session session) -> sftp_session {
                       sftp_session sftp =
                           static_cast<sftp_session_struct*>(std::calloc(1, sizeof(struct sftp_session_struct)));
                       return sftp;
                   }},
          free_sftp{mock_sftp_free, [](sftp_session sftp) { std::free(sftp); }}
    {
        close.returnValue(SSH_OK);
    }

    mp::SFTPClient make_sftp_client()
    {
        return {std::make_unique<mp::SSHSession>("b", 43)};
    }

    decltype(MOCK(sftp_close)) close{MOCK(sftp_close)};
    MockScope<decltype(mock_sftp_new)> sftp_new;
    MockScope<decltype(mock_sftp_free)> free_sftp;

    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    std::stringstream test_stream{"testing stream :-)"};
};
} // namespace

// testing sftp_session

TEST_F(SFTPClient, throws_when_unable_to_allocate_sftp_session)
{
    REPLACE(sftp_new, [](auto...) { return nullptr; });

    EXPECT_THROW(make_sftp_client(), std::runtime_error);
}

TEST_F(SFTPClient, throws_when_failed_to_init)
{
    REPLACE(sftp_init, [](auto...) { return SSH_ERROR; });

    EXPECT_THROW(make_sftp_client(), std::runtime_error);
}

// testing push method

TEST_F(SFTPClient, push_throws_on_sftp_open_failed)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        session->errnum = SSH_ERROR;
        return nullptr;
    });

    auto sftp = make_sftp_client();

    EXPECT_THROW(sftp.push_file("foo", "bar"), std::runtime_error);
}

TEST_F(SFTPClient, push_throws_on_invalid_source)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](auto...) { return get_dummy_sftp_file(); });

    auto sftp = make_sftp_client();

    EXPECT_THROW(sftp.push_file("foo", "bar"), std::runtime_error);
}

TEST_F(SFTPClient, push_throws_on_sftp_write_error)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        sftp_file file = get_dummy_sftp_file();
        file->sftp = session;
        return file;
    });
    REPLACE(sftp_write, [](sftp_file file, auto...) {
        file->sftp->errnum = SSH_ERROR;
        return -1;
    });

    auto sftp = make_sftp_client();

    EXPECT_THROW(sftp.push_file(file_name.toStdString(), "bar"), std::runtime_error);
}

TEST_F(SFTPClient, pull_throws_on_sftp_open_failed)
{
    const std::string source_path{"foo"};

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        session->errnum = SSH_ERROR;
        return nullptr;
    });

    auto sftp = make_sftp_client();

    EXPECT_THROW(sftp.pull_file(source_path, "bar"), std::runtime_error);
}

TEST_F(SFTPClient, pull_throws_on_sftp_read_failed)
{
    const std::string source_path{"foo"};

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        auto file = get_dummy_sftp_file();
        file->sftp = session;
        return file;
    });
    REPLACE(sftp_read, [](sftp_file file, auto...) {
        file->sftp->errnum = SSH_ERROR;
        return -1;
    });

    auto sftp = make_sftp_client();

    EXPECT_THROW(sftp.pull_file(source_path, "bar"), std::runtime_error);
}

// testing stream method

TEST_F(SFTPClient, in_steam_throws_on_sftp_open_failed)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        session->errnum = SSH_ERROR;
        return nullptr;
    });

    auto sftp = make_sftp_client();

    std::istream fake_cin{test_stream.rdbuf()};
    EXPECT_THROW(sftp.stream_file("bar", fake_cin), std::runtime_error);
}

TEST_F(SFTPClient, in_stream_throws_on_write_failed)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        auto file = get_dummy_sftp_file();
        file->sftp = session;
        return file;
    });
    REPLACE(sftp_write, [](sftp_file file, auto...) {
        file->sftp->errnum = SSH_ERROR;
        return -1;
    });

    auto sftp = make_sftp_client();

    std::istream fake_cin{test_stream.rdbuf()};
    EXPECT_THROW(sftp.stream_file("bar", fake_cin), std::runtime_error);
}

TEST_F(SFTPClient, out_stream_throws_on_sftp_open_failed)
{
    const std::string source_path{"bar"};

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        session->errnum = SSH_ERROR;
        return nullptr;
    });

    auto sftp = make_sftp_client();

    std::ostream fake_cout{test_stream.rdbuf()};
    EXPECT_THROW(sftp.stream_file(source_path, fake_cout), std::runtime_error);
}

TEST_F(SFTPClient, out_stream_throws_on_sftp_read_failed)
{
    const std::string source_path{"bar"};

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        auto file = get_dummy_sftp_file();
        file->sftp = session;
        return file;
    });
    REPLACE(sftp_read, [](sftp_file file, auto...) {
        file->sftp->errnum = SSH_ERROR;
        return -1;
    });

    auto sftp = make_sftp_client();

    std::ostream fake_cout{test_stream.rdbuf()};
    EXPECT_THROW(sftp.stream_file(source_path, fake_cout), std::runtime_error);
}

TEST_F(SFTPClient, out_stream_writes_data_with_nulls)
{
    using namespace std::string_literals;

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](auto...) { return nullptr; });

    static const auto source_data = "a\0b\0\xab c"s;
    static const auto data_size = source_data.size();
    auto have_read = false;

    REPLACE(sftp_read, [&have_read](sftp_file /*file*/, void* buf, size_t count) -> ssize_t {
        EXPECT_GE(count, data_size);

        if (have_read)
            return 0L;

        have_read = true;
        memcpy(buf, source_data.data(), data_size);
        return data_size;
    });

    auto sftp = make_sftp_client();
    std::ostringstream out;

    EXPECT_NO_THROW(sftp.stream_file("fake path", out));
    EXPECT_EQ(out.str(), source_data);
}

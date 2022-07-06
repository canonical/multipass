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

sftp_attributes get_dummy_sftp_attr()
{
    return static_cast<sftp_attributes_struct*>(calloc(1, sizeof(struct sftp_attributes_struct)));
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
    REPLACE(sftp_stat, [](auto...) {
        auto attr = get_dummy_sftp_attr();
        attr->type = SSH_FILEXFER_TYPE_REGULAR;
        return attr;
    });
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

TEST_F(SFTPClient, push_throws_on_overwrite_directory_with_file)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto, const char* path) {
        auto attr = get_dummy_sftp_attr();
        attr->type = (strcmp(path, "bar") == 0 || strcmp(path, "bar/foo") == 0) ? SSH_FILEXFER_TYPE_DIRECTORY
                                                                                : SSH_FILEXFER_TYPE_REGULAR;
        return attr;
    });

    auto sftp = make_sftp_client();

    MP_EXPECT_THROW_THAT(
        sftp.push_file("foo", "bar"), std::runtime_error,
        mpt::match_what(testing::StrEq("[sftp] cannot overwrite remote directory 'bar/foo' with non-directory")));
}

TEST_F(SFTPClient, push_throws_on_target_not_exists)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return nullptr; });

    auto sftp = make_sftp_client();

    MP_EXPECT_THROW_THAT(sftp.push_file("foo", "bar"), std::runtime_error,
                         mpt::match_what(testing::StrEq("[sftp] remote target does not exist")));
}

TEST_F(SFTPClient, push_success)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/foo";
    auto file_size = mpt::make_file_with_content(file_name);

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto, const char* path) {
        auto attr = get_dummy_sftp_attr();
        if (strcmp(path, "bar") == 0)
        {
            attr->type = SSH_FILEXFER_TYPE_DIRECTORY;
        }
        else if (strcmp(path, "bar/foo") == 0 || strcmp(path, "baz") == 0)
        {
            attr->type = SSH_FILEXFER_TYPE_REGULAR;
        }
        return attr;
    });

    REPLACE(sftp_open, [](auto sftp, auto...) {
        auto file = get_dummy_sftp_file();
        file->sftp = sftp;
        return file;
    });

    REPLACE(sftp_write, [file_size](auto...) { return file_size; });

    auto sftp = make_sftp_client();

    EXPECT_NO_THROW(sftp.push_file(file_name.toStdString(), "bar"));
    EXPECT_NO_THROW(sftp.push_file(file_name.toStdString(), "baz"));
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

TEST_F(SFTPClient, pull_throws_on_overwrite_directory_with_file)
{
    QTemporaryDir temp_dir{};
    QDir dir{temp_dir.path()};
    dir.mkdir("foo");
    dir.cd("foo");

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    auto sftp = make_sftp_client();

    MP_EXPECT_THROW_THAT(sftp.pull_file("foo", temp_dir.path().toStdString()), std::runtime_error,
                         mpt::match_what(testing::StrEq(fmt::format(
                             "[sftp] cannot overwrite local directory '{}' with non-directory", dir.path()))));
}

TEST_F(SFTPClient, pull_throws_on_target_not_exists)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    auto sftp = make_sftp_client();

    MP_EXPECT_THROW_THAT(sftp.pull_file("foo", "/non/existent/path/bar"), std::runtime_error,
                         mpt::match_what(testing::StrEq("[sftp] local target does not exist")));
}

TEST_F(SFTPClient, pull_success)
{
    QTemporaryDir temp_dir;
    const auto file_path1 = temp_dir.filePath("bar").toStdString();
    const auto file_path2 = temp_dir.path().toStdString();
    const auto test_data = "test";

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_open, [](auto sftp, auto...) {
        auto file = get_dummy_sftp_file();
        file->sftp = sftp;
        return file;
    });

    auto allow_read = true;
    REPLACE(sftp_read, [&](auto, void* buf, auto) mutable {
        if (allow_read)
        {
            int size = strlen(test_data);
            memcpy(buf, test_data, size);
            allow_read = false;
            return size;
        }
        else
        {
            return 0;
        }
    });

    auto sftp = make_sftp_client();

    EXPECT_NO_THROW(sftp.pull_file("foo", file_path1));
    QFile test_file1{file_path1.c_str()};
    ASSERT_TRUE(test_file1.exists());
    test_file1.open(QIODevice::ReadOnly);
    EXPECT_EQ(test_data, test_file1.readAll());

    allow_read = true;
    EXPECT_NO_THROW(sftp.pull_file("foo", file_path2));
    QFile test_file2{(file_path2 + "/foo").c_str()};
    ASSERT_TRUE(test_file2.exists());
    test_file2.open(QIODevice::ReadOnly);
    EXPECT_EQ(test_data, test_file2.readAll());
}

// testing stream method

TEST_F(SFTPClient, in_steam_throws_on_sftp_open_failed)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) {
        auto attr = get_dummy_sftp_attr();
        attr->type = SSH_FILEXFER_TYPE_REGULAR;
        return attr;
    });
    REPLACE(sftp_open, [](sftp_session session, auto...) {
        session->errnum = SSH_ERROR;
        return nullptr;
    });

    auto sftp = make_sftp_client();

    std::istream fake_cin{test_stream.rdbuf()};
    EXPECT_THROW(sftp.from_cin(fake_cin, "bar"), std::runtime_error);
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
    EXPECT_THROW(sftp.from_cin(fake_cin, "bar"), std::runtime_error);
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
    EXPECT_THROW(sftp.to_cout(source_path, fake_cout), std::runtime_error);
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
    EXPECT_THROW(sftp.to_cout(source_path, fake_cout), std::runtime_error);
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

    EXPECT_NO_THROW(sftp.to_cout("fake path", out));
    EXPECT_EQ(out.str(), source_data);
}

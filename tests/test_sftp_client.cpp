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
#include "mock_file_ops.h"
#include "mock_recursive_dir_iterator.h"
#include "mock_sftp.h"
#include "mock_sftp_dir_iterator.h"
#include "mock_sftp_utils.h"
#include "mock_ssh_test_fixture.h"
#include "path.h"

#include <multipass/ssh/sftp_client.h>
#include <multipass/ssh/ssh_session.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
sftp_file get_dummy_sftp_file(sftp_session sftp = nullptr)
{
    auto file = static_cast<sftp_file_struct*>(calloc(1, sizeof(struct sftp_file_struct)));
    file->sftp = sftp;
    return file;
}

sftp_attributes get_dummy_sftp_attr(uint8_t type = SSH_FILEXFER_TYPE_REGULAR, const std::string& name = "",
                                    mode_t perms = 0777)
{
    auto attr = static_cast<sftp_attributes_struct*>(calloc(1, sizeof(struct sftp_attributes_struct)));
    attr->type = type;
    attr->name = strdup(name.c_str());
    attr->permissions = perms;
    return attr;
}

struct SFTPClient : public testing::Test
{
    SFTPClient()
        : sftp_new{mock_sftp_new,
                   [](ssh_session session) -> sftp_session {
                       auto sftp =
                           static_cast<sftp_session_struct*>(std::calloc(1, sizeof(struct sftp_session_struct)));
                       return sftp;
                   }},
          free_sftp{mock_sftp_free, [](sftp_session sftp) { std::free(sftp); }}
    {
        close.returnValue(SSH_OK);
    }

    static mp::SFTPClient make_sftp_client()
    {
        return {std::make_unique<mp::SSHSession>("b", 43)};
    }

    decltype(MOCK(sftp_close)) close{MOCK(sftp_close)};
    MockScope<decltype(mock_sftp_new)> sftp_new;
    MockScope<decltype(mock_sftp_free)> free_sftp;

    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    std::stringstream test_stream{"testing stream :-)"};

    mpt::MockFileOps::GuardedMock mock_file_ops_guard{mpt::MockFileOps::inject()};
    mpt::MockFileOps* mock_file_ops{mock_file_ops_guard.first};
    mpt::MockSFTPUtils::GuardedMock mock_sftp_utils_guard{mpt::MockSFTPUtils::inject()};
    mpt::MockSFTPUtils* mock_sftp_utils{mock_sftp_utils_guard.first};
    fs::path source_path = "source/path";
    fs::path target_path = "target/path";
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

TEST_F(SFTPClient, is_dir)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });

    auto sftp_stat_type = [](auto type) { return get_dummy_sftp_attr(type); };

    auto mocked_sftp_stat = MOCK(sftp_stat);
    auto sftp_client = make_sftp_client();

    mocked_sftp_stat.returnValue(nullptr);
    EXPECT_FALSE(sftp_client.is_dir("non/existent/path"));
    mocked_sftp_stat.returnValue(sftp_stat_type(SSH_FILEXFER_TYPE_DIRECTORY));
    EXPECT_TRUE(sftp_client.is_dir("a/true/directory"));
    mocked_sftp_stat.returnValue(sftp_stat_type(SSH_FILEXFER_TYPE_REGULAR));
    EXPECT_FALSE(sftp_client.is_dir("not/a/directory"));
}

TEST_F(SFTPClient, push_file_success)
{
    std::string test_data = "test_data";

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_file_target(_, source_path, target_path))
        .WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_read(source_path))
        .WillOnce(Return(std::make_unique<std::stringstream>(test_data)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    std::string written_data;
    REPLACE(sftp_write, [&](auto, auto data, auto size) { return written_data.append((char*)data, size).size(); });

    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(); });
    fs::file_status source_status{fs::file_type::regular, fs::perms::all};
    EXPECT_CALL(*mock_file_ops, status(source_path, _)).WillOnce(Return(source_status));
    mode_t written_perms;
    REPLACE(sftp_setstat, [&](auto, auto, auto attr) {
        written_perms = attr->permissions;
        return SSH_FX_OK;
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.push(source_path, target_path, {}, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
    EXPECT_EQ(test_data, written_data);
    EXPECT_EQ(static_cast<mode_t>(source_status.permissions()), written_perms);
}

TEST_F(SFTPClient, push_file_cannot_open_source)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_file_target(_, source_path, target_path))
        .WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_read(source_path)).WillOnce([&](auto...) {
        auto file = std::make_unique<std::stringstream>();
        file->setstate(std::ios_base::failbit);
        errno = EACCES;
        return file;
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot open local file 'source/path': Permission denied"));
}

TEST_F(SFTPClient, push_file_cannot_open_target)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_file_target(_, source_path, target_path))
        .WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_read(source_path)).WillOnce(Return(std::make_unique<std::stringstream>()));
    REPLACE(sftp_open, [](auto...) { return nullptr; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot open remote file 'target/path': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, push_file_cannot_write_target)
{
    std::string test_data = "test_data";

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_file_target(_, source_path, target_path))
        .WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_read(source_path))
        .WillOnce(Return(std::make_unique<std::stringstream>(test_data)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    REPLACE(sftp_write, [](auto...) { return -1; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot write to remote file 'target/path': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, push_file_cannot_read_source)
{
    std::string test_data = "test_data";

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_file_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto test_file = std::make_unique<std::stringstream>(test_data);
    auto test_file_p = test_file.get();
    EXPECT_CALL(*mock_file_ops, open_read(source_path)).WillOnce(Return(std::move(test_file)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    REPLACE(sftp_write, [](auto, auto, auto size) { return size; });
    REPLACE(sftp_stat, [&](auto...) {
        test_file_p->clear();
        test_file_p->setstate(std::ios_base::failbit);
        errno = EACCES;
        return get_dummy_sftp_attr();
    });
    EXPECT_CALL(*mock_file_ops, status(source_path, _))
        .WillOnce(Return(fs::file_status{fs::file_type::regular, fs::perms::all}));
    REPLACE(sftp_setstat, [](auto...) { return SSH_FX_OK; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot read from local file 'source/path': Permission denied"));
}

TEST_F(SFTPClient, push_file_cannot_set_perms)
{
    std::string test_data = "test_data";

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_file_target(_, source_path, target_path))
        .WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_read(source_path))
        .WillOnce(Return(std::make_unique<std::stringstream>(test_data)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    REPLACE(sftp_write, [](auto, auto, auto size) { return size; });

    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(); });
    EXPECT_CALL(*mock_file_ops, status(source_path, _))
        .WillOnce(Return(fs::file_status{fs::file_type::regular, fs::perms::all}));
    REPLACE(sftp_setstat, [](auto...) { return -1; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, {}, err_sink));
    EXPECT_THAT(
        err_sink.str(),
        HasSubstr("[sftp] cannot set permissions for remote file 'target/path': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, pull_file_success)
{
    std::string test_data = "test_data";

    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_file_target(source_path, target_path)).WillOnce(Return(target_path));

    auto test_file = std::make_unique<std::stringstream>();
    auto test_file_p = test_file.get();
    EXPECT_CALL(*mock_file_ops, open_write(target_path)).WillOnce(Return(std::move(test_file)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    auto mocked_sftp_read = [&, read = false](auto, void* data, auto) mutable {
        strcpy((char*)data, test_data.c_str());
        return (read = !read) ? test_data.size() : 0;
    };
    REPLACE(sftp_read, mocked_sftp_read);

    mode_t perms = 0777;
    REPLACE(sftp_stat, [&](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_REGULAR, "", perms); });
    fs::perms written_perms;
    EXPECT_CALL(*mock_file_ops, permissions(target_path, static_cast<fs::perms>(perms), _))
        .WillOnce([&](auto, auto perms, auto) { written_perms = perms; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.pull(source_path, target_path, {}, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
    EXPECT_EQ(test_data, test_file_p->str());
    EXPECT_EQ(static_cast<fs::perms>(perms), written_perms);
}

TEST_F(SFTPClient, pull_file_cannot_open_source)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_file_target(source_path, target_path)).WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_write(target_path)).WillOnce(Return(std::make_unique<std::stringstream>()));
    REPLACE(sftp_open, [](auto...) { return nullptr; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot open remote file 'source/path': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, pull_file_cannot_open_target)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_file_target(source_path, target_path)).WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_write(target_path)).WillOnce([&](auto...) {
        auto file = std::make_unique<std::stringstream>();
        file->setstate(std::ios_base::failbit);
        errno = EACCES;
        return file;
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot open local file 'target/path': Permission denied"));
}

TEST_F(SFTPClient, pull_file_cannot_write_target)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_file_target(source_path, target_path)).WillOnce(Return(target_path));

    auto test_file = std::make_unique<std::stringstream>();
    auto test_file_p = test_file.get();
    EXPECT_CALL(*mock_file_ops, open_write(target_path)).WillOnce(Return(std::move(test_file)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    auto mocked_sftp_read = [&, read = false](auto...) mutable {
        test_file_p->clear();
        test_file_p->setstate(std::ios_base::failbit);
        errno = EACCES;
        return (read = !read) ? 10 : 0;
    };
    REPLACE(sftp_read, mocked_sftp_read);
    REPLACE(sftp_stat, [&](auto...) { return get_dummy_sftp_attr(); });
    EXPECT_CALL(*mock_file_ops, permissions(target_path, _, _));
    REPLACE(sftp_setstat, [](auto...) { return SSH_FX_OK; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot write to local file 'target/path': Permission denied"));
}

TEST_F(SFTPClient, pull_file_cannot_read_source)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_file_target(source_path, target_path)).WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_write(target_path)).WillOnce(Return(std::make_unique<std::stringstream>()));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    REPLACE(sftp_read, [](auto...) { return -1; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot read from remote file 'source/path': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, pull_file_cannot_set_perms)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_file_target(source_path, target_path)).WillOnce(Return(target_path));
    EXPECT_CALL(*mock_file_ops, open_write(target_path)).WillOnce(Return(std::make_unique<std::stringstream>()));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });
    REPLACE(sftp_read, [read = false](auto...) mutable { return (read = !read) ? 10 : 0; });

    mode_t perms = 0777;
    REPLACE(sftp_stat, [&](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_REGULAR, "", perms); });
    EXPECT_CALL(*mock_file_ops, permissions(target_path, static_cast<fs::perms>(perms), _))
        .WillOnce([&](auto, auto, std::error_code& err) { err = std::make_error_code(std::errc::permission_denied); });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot set permissions for local file 'target/path': Permission denied"));
}

TEST_F(SFTPClient, push_dir_success_regular)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::regular, fs::perms::all};
    fs::path path{"file"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));

    std::string test_data = "test_data";
    EXPECT_CALL(*mock_file_ops, open_read).WillOnce(Return(std::make_unique<std::stringstream>(test_data)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });
    std::string written_data;
    REPLACE(sftp_write,
            [&](auto, const void* data, auto size) { return written_data.append((char*)data, size).size(); });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(); });
    EXPECT_CALL(*mock_file_ops, status).WillOnce(Return(status));

    mode_t written_perms;
    REPLACE(sftp_setstat, [&](auto, auto, auto attr) {
        written_perms = attr->permissions;
        return SSH_FX_OK;
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
    EXPECT_EQ(status.permissions(), static_cast<fs::perms>(written_perms));
    EXPECT_EQ(test_data, written_data);
}

TEST_F(SFTPClient, push_dir_success_dir)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::directory, fs::perms::all};
    fs::path path{"dir"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
}

TEST_F(SFTPClient, push_dir_fail_dir)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::directory, fs::perms::all};
    fs::path path{source_path / "dir"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));
    REPLACE(sftp_mkdir, [](auto...) { return -1; });
    REPLACE(sftp_get_error, [](auto...) { return SSH_FX_PERMISSION_DENIED; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot create remote directory 'target/path/dir': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, push_dir_success_symlink)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::symlink, fs::perms::all};
    fs::path path{"symlink"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));

    EXPECT_CALL(*mock_file_ops, read_symlink);
    REPLACE(sftp_lstat, [](auto...) { return get_dummy_sftp_attr(); });
    REPLACE(sftp_unlink, [](auto...) { return SSH_FX_OK; });
    REPLACE(sftp_symlink, [](auto...) { return SSH_FX_OK; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
}

TEST_F(SFTPClient, push_dir_cannot_read_symlink)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::symlink, fs::perms::all};
    fs::path path{source_path / "symlink"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));

    EXPECT_CALL(*mock_file_ops, read_symlink).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return "";
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot read local link 'source/path/symlink': Permission denied"));
}

TEST_F(SFTPClient, push_dir_cannot_create_symlink)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::symlink, fs::perms::all};
    fs::path path{source_path / "symlink"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));

    EXPECT_CALL(*mock_file_ops, read_symlink);
    REPLACE(sftp_lstat, [](auto...) { return get_dummy_sftp_attr(); });
    REPLACE(sftp_unlink, [](auto...) { return SSH_FX_OK; });
    REPLACE(sftp_symlink, [](auto...) { return -1; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot create remote symlink 'target/path/symlink': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, push_dir_symlink_over_dir)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::symlink, fs::perms::all};
    fs::path path{source_path / "symlink"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));

    EXPECT_CALL(*mock_file_ops, read_symlink);
    REPLACE(sftp_lstat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot overwrite remote directory 'target/path/symlink' with non-directory"));
}

TEST_F(SFTPClient, push_dir_unknown_file_type)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));

    mpt::MockDirectoryEntry entry;
    auto status = fs::file_status{fs::file_type::unknown, fs::perms::all};
    fs::path path{source_path / "unknown"};
    EXPECT_CALL(entry, path).WillRepeatedly(ReturnRef(path));
    EXPECT_CALL(entry, symlink_status()).WillRepeatedly(Return(status));
    EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry));

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot copy 'source/path/unknown': not a regular file"));
}

TEST_F(SFTPClient, push_dir_open_iter_fail)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_sftp_utils, get_full_remote_dir_target(_, source_path, target_path))
        .WillOnce(Return(target_path));

    EXPECT_CALL(*mock_file_ops, recursive_dir_iterator(source_path, _)).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return std::make_unique<mpt::MockRecursiveDirIterator>();
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot open local directory 'source/path': Permission denied"));
}

TEST_F(SFTPClient, push_dir_cannot_access_target)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return false;
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot access 'source/path': Permission denied"));
}

TEST_F(SFTPClient, push_dir_r_not_specified)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_file_ops, is_directory(source_path, _)).WillOnce(Return(true));

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.push(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] omitting local directory 'source/path': -r not specified"));
}

TEST_F(SFTPClient, pull_dir_success_regular)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_REGULAR, source_path / "file"))));

    std::string test_data = "test_data";
    auto test_file = std::make_unique<std::stringstream>();
    auto test_file_p = test_file.get();
    EXPECT_CALL(*mock_file_ops, open_write).WillOnce(Return(std::move(test_file)));
    REPLACE(sftp_open, [](auto sftp, auto...) { return get_dummy_sftp_file(sftp); });

    auto mocked_sftp_read = [&, read = false](auto, const void* data, auto size) mutable {
        strcpy((char*)data, test_data.c_str());
        return (read = !read) ? test_data.size() : 0;
    };
    REPLACE(sftp_read, mocked_sftp_read);

    mode_t perms = 0777;
    REPLACE(sftp_stat, [&](auto, auto path) {
        return source_path == path ? get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY)
                                   : get_dummy_sftp_attr(SSH_FILEXFER_TYPE_REGULAR, "", perms);
    });
    fs::perms written_perms;
    EXPECT_CALL(*mock_file_ops, permissions(target_path / "file", static_cast<fs::perms>(perms), _))
        .WillOnce([&](auto, auto perms, auto) { written_perms = perms; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
    EXPECT_EQ(test_data, test_file_p->str());
    EXPECT_EQ(static_cast<fs::perms>(perms), written_perms);
}

TEST_F(SFTPClient, pull_dir_success_dir)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY, "source/path/dir"))));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path / "dir", _));

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
}

TEST_F(SFTPClient, pull_dir_fail_dir)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY, source_path / "dir"))));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path / "dir", _)).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return false;
    });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot create local directory 'target/path/dir': Permission denied"));
}

TEST_F(SFTPClient, pull_dir_success_symlink)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_SYMLINK, source_path / "symlink"))));

    REPLACE(sftp_readlink, [](auto...) { return (char*)malloc(10); });
    EXPECT_CALL(*mock_file_ops, is_directory).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, remove(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, create_symlink(_, target_path / "symlink", _));

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_TRUE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_TRUE(err_sink.str().empty());
}

TEST_F(SFTPClient, pull_dir_cannot_read_symlink)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_SYMLINK, source_path / "symlink"))));

    REPLACE(sftp_readlink, [](auto...) { return nullptr; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot read remote link 'source/path/symlink': SFTP server: Permission denied"));
}

TEST_F(SFTPClient, pull_dir_cannot_create_symlink)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_SYMLINK, source_path / "symlink"))));

    REPLACE(sftp_readlink, [](auto...) { return (char*)malloc(10); });
    EXPECT_CALL(*mock_file_ops, is_directory).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, remove(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, create_symlink(_, target_path / "symlink", _))
        .WillOnce([](auto, auto, std::error_code& err) { err = std::make_error_code(std::errc::permission_denied); });
    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot create local symlink 'target/path/symlink': Permission denied"));
}

TEST_F(SFTPClient, pull_dir_symlink_over_dir)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_SYMLINK, source_path / "symlink"))));

    REPLACE(sftp_readlink, [](auto...) { return (char*)malloc(10); });
    EXPECT_CALL(*mock_file_ops, is_directory).WillOnce(Return(true));

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(),
                HasSubstr("[sftp] cannot overwrite local directory 'target/path/symlink' with non-directory"));
}

TEST_F(SFTPClient, pull_dir_unknown_file_type)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });
    EXPECT_CALL(*mock_sftp_utils, get_full_local_dir_target(source_path, target_path)).WillOnce(Return(target_path));

    auto iter = std::make_unique<mpt::MockSFTPDirIterator>();
    auto iter_p = iter.get();
    EXPECT_CALL(*mock_sftp_utils, make_SFTPDirIterator(_, source_path)).WillOnce(Return(std::move(iter)));
    EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillRepeatedly(Return(false));
    EXPECT_CALL(*iter_p, next)
        .WillOnce(Return(std::unique_ptr<sftp_attributes_struct>(
            get_dummy_sftp_attr(SSH_FILEXFER_TYPE_UNKNOWN, source_path / "unknown"))));

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, mp::TransferFlags::Recursive, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] cannot copy 'source/path/unknown': not a regular file"));
}

TEST_F(SFTPClient, pull_dir_r_not_specified)
{
    REPLACE(sftp_init, [](auto...) { return SSH_OK; });
    REPLACE(sftp_stat, [](auto...) { return get_dummy_sftp_attr(SSH_FILEXFER_TYPE_DIRECTORY); });

    auto sftp_client = make_sftp_client();

    std::stringstream err_sink;
    EXPECT_FALSE(sftp_client.pull(source_path, target_path, {}, err_sink));
    EXPECT_THAT(err_sink.str(), HasSubstr("[sftp] omitting remote directory 'source/path': -r not specified"));
}

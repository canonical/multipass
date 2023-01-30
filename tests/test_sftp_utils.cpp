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
#include "mock_file_ops.h"
#include "mock_sftp.h"
#include "mock_ssh.h"

#include <multipass/ssh/sftp_utils.h>

#include <fmt/std.h>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace fs = mp::fs;

using namespace testing;

namespace
{
auto get_dummy_attr(const char* name, uint8_t type)
{
    auto attr = static_cast<sftp_attributes_struct*>(calloc(1, sizeof(struct sftp_attributes_struct)));
    attr->name = strdup(name);
    attr->type = type;
    return attr;
}
} // namespace

struct SFTPUtils : testing::Test
{
    mpt::MockFileOps::GuardedMock mock_file_ops_guard{mpt::MockFileOps::inject()};
    mpt::MockFileOps* mock_file_ops{mock_file_ops_guard.first};
    fs::path source_path = "source/path";
    fs::path target_path = "target/path";
};

TEST_F(SFTPUtils, get_full_local_file_target__target_is_dir_child_is_not)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path / source_path.filename(), _)).WillOnce(Return(false));

    EXPECT_EQ(MP_SFTPUTILS.get_local_file_target(source_path, target_path, false),
              target_path / source_path.filename());
}

TEST_F(SFTPUtils, get_full_local_file_target__target_exists_not_dir)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(false));

    EXPECT_EQ(MP_SFTPUTILS.get_local_file_target(source_path, target_path, false), target_path);
}

TEST_F(SFTPUtils, get_full_local_file_target__target_not_exists_parent_does)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, exists(target_path.parent_path(), _)).WillOnce(Return(true));

    EXPECT_EQ(MP_SFTPUTILS.get_local_file_target(source_path, target_path, false), target_path);
}

TEST_F(SFTPUtils, get_full_local_file_target__target_not_exists_parent_does_recursive_fail)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(false));
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, create_directories(target_path.parent_path(), _))
        .WillOnce([&](auto, std::error_code& e) {
            e = err;
            return false;
        });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_file_target(source_path, target_path, true), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot create local directory {}: {}",
                                                           target_path.parent_path(), err.message()))));
}

TEST_F(SFTPUtils, get_full_local_file_target__target_not_exists_parent_neither)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, exists(target_path.parent_path(), _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_file_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq("local target does not exist")));
}

TEST_F(SFTPUtils, get_full_local_file_target__target_is_dir_child_is_too)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path / source_path.filename(), _)).WillOnce(Return(true));

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_file_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot overwrite local directory {} with non-directory",
                                                           target_path / source_path.filename()))));
}

TEST_F(SFTPUtils, get_full_local_file_target__cannot_access_target)
{
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce([&](auto, std::error_code& e) {
        e = err;
        return false;
    });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_file_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot access {}: {}", target_path, err.message()))));
}

TEST_F(SFTPUtils, get_full_local_file_target__cannot_access_parent)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(false));
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, exists(target_path.parent_path(), _)).WillOnce([&](auto, std::error_code& e) {
        e = err;
        return false;
    });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_local_file_target(source_path, target_path, false), mp::SFTPError,
        mpt::match_what(StrEq(fmt::format("cannot access {}: {}", target_path.parent_path(), err.message()))));
}

TEST_F(SFTPUtils, get_full_local_file_target__cannot_access_child)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(true));
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, is_directory(target_path / source_path.filename(), _))
        .WillOnce([&](auto, std::error_code& e) {
            e = err;
            return false;
        });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_file_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot access {}: {}", target_path / source_path.filename(),
                                                           err.message()))));
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_is_dir_child_is_not)
{
    REPLACE(sftp_stat, [&](auto, auto path) -> sftp_attributes {
        if (target_path == path)
            return get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY);
        if (target_path / source_path.filename() == path)
            return get_dummy_attr(path, SSH_FILEXFER_TYPE_REGULAR);
        return nullptr;
    });

    EXPECT_EQ(MP_SFTPUTILS.get_remote_file_target(nullptr, source_path, target_path, false),
              target_path / source_path.filename());
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_exists_not_dir)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? get_dummy_attr(path, SSH_FILEXFER_TYPE_REGULAR) : nullptr;
    });

    EXPECT_EQ(MP_SFTPUTILS.get_remote_file_target(nullptr, source_path, target_path, false), target_path);
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_not_exists_parent_does)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? nullptr : get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY);
    });

    EXPECT_EQ(MP_SFTPUTILS.get_remote_file_target(nullptr, source_path, target_path, false), target_path);
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_not_exists_parent_does_recursive)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? nullptr : get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY);
    });
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    EXPECT_EQ(MP_SFTPUTILS.get_remote_file_target(nullptr, source_path, target_path, true), target_path);
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_not_exists_parent_neither)
{
    REPLACE(sftp_stat, [](auto...) { return nullptr; });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_remote_file_target(nullptr, source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq("remote target does not exist")));
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_is_dir_child_is_too)
{
    REPLACE(sftp_stat, [](auto, auto path) { return get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY); });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_remote_file_target(nullptr, source_path, target_path, false), mp::SFTPError,
        mpt::match_what(StrEq(fmt::format("cannot overwrite remote directory \"{}\" with non-directory",
                                          target_path.u8string() + '/' + source_path.filename().u8string()))));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_exists_not_dir)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), mp::SFTPError,
        mpt::match_what(StrEq(fmt::format("cannot overwrite local non-directory {} with directory", target_path))));
}

TEST_F(SFTPUtils, get_full_local_dir_target__cannot_access_target)
{
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce([&](auto, std::error_code& e) {
        e = err;
        return false;
    });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot access {}: {}", target_path, err.message()))));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_not_exists_can_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path, _)).WillOnce(Return(true));

    EXPECT_EQ(MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), target_path);
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_not_exists_cannot_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(false));
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, create_directory(target_path, _)).WillOnce([&](auto, std::error_code& e) {
        e = err;
        return false;
    });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), mp::SFTPError,
        mpt::match_what(StrEq(fmt::format("cannot create local directory {}: {}", target_path, err.message()))));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_child_is_not)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path / source_path.filename(), _)).WillRepeatedly(Return(false));

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot overwrite local non-directory {} with directory",
                                                           target_path / source_path.filename()))));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_child_not_exists_can_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _)).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path / source_path.filename(), _)).WillRepeatedly(Return(true));

    EXPECT_EQ(MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), target_path / source_path.filename());
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_child_not_exists_cannot_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _)).WillRepeatedly(Return(false));
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, create_directory(target_path / source_path.filename(), _))
        .WillOnce([&](auto, std::error_code& e) {
            e = err;
            return false;
        });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot create local directory {}: {}",
                                                           target_path / source_path.filename(), err.message()))));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_cannot_access_child)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _))
        .WillRepeatedly([&](auto, std::error_code& e) {
            e = err;
            return false;
        });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_local_dir_target(source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format("cannot access {}: {}", target_path / source_path.filename(),
                                                           err.message()))));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_exists_not_dir)
{
    REPLACE(sftp_stat, [](auto, auto path) { return get_dummy_attr(path, SSH_FILEXFER_TYPE_REGULAR); });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_remote_dir_target(nullptr, source_path, target_path, false), mp::SFTPError,
        mpt::match_what(StrEq(fmt::format("cannot overwrite remote non-directory {} with directory", target_path))));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_not_exists_can_create)
{
    REPLACE(sftp_stat, [](auto...) { return nullptr; });
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    EXPECT_EQ(MP_SFTPUTILS.get_remote_dir_target(nullptr, source_path, target_path, false), target_path);
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_not_exists_can_create_recursive)
{
    REPLACE(sftp_stat, [](auto...) { return nullptr; });
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    EXPECT_EQ(MP_SFTPUTILS.get_remote_dir_target(nullptr, source_path, target_path, true), target_path);
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_not_exists_cannot_create)
{
    REPLACE(sftp_stat, [](auto...) { return nullptr; });
    REPLACE(sftp_mkdir, [](auto...) { return -1; });
    auto err = "SFTP server: Permission denied";
    REPLACE(ssh_get_error, [&](auto...) { return err; });

    sftp_session_struct sftp{};
    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_remote_dir_target(&sftp, source_path, target_path, false), mp::SFTPError,
        mpt::match_what(StrEq(fmt::format("cannot create remote directory {}: {}", target_path, err))));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_is_dir_child_is_not)
{
    auto target_child_path = target_path.u8string() + '/' + source_path.filename().u8string();
    REPLACE(sftp_stat, [&](auto, auto path) -> sftp_attributes {
        if (target_path == path)
            return get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY);
        if (target_child_path == path)
            return get_dummy_attr(path, SSH_FILEXFER_TYPE_REGULAR);
        return nullptr;
    });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_remote_dir_target(nullptr, source_path, target_path, false), mp::SFTPError,
                         mpt::match_what(StrEq(fmt::format(
                             "cannot overwrite remote non-directory \"{}\" with directory", target_child_path))));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_is_dir_child_not_exists_can_create)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY) : nullptr;
    });
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    EXPECT_EQ(MP_SFTPUTILS.get_remote_dir_target(nullptr, source_path, target_path, false),
              target_path / source_path.filename());
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_is_dir_child_not_exists_cannot_create)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY) : nullptr;
    });
    REPLACE(sftp_mkdir, [](auto...) { return -1; });
    auto err = "SFTP server: Permission denied";
    REPLACE(ssh_get_error, [&](auto...) { return err; });

    sftp_session_struct sftp{};
    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_remote_dir_target(&sftp, source_path, target_path, false), mp::SFTPError,
        mpt::match_what(StrEq(fmt::format("cannot create remote directory \"{}\": {}",
                                          target_path.u8string() + '/' + source_path.filename().u8string(), err))));
}

TEST_F(SFTPUtils, mkdir_success)
{
    REPLACE(sftp_lstat, [](auto...) { return nullptr; });
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    EXPECT_NO_THROW(MP_SFTPUTILS.mkdir_recursive(nullptr, "some/nested/path"));
}

TEST_F(SFTPUtils, mkdir_cannot_overwrite_non_directory)
{
    REPLACE(sftp_lstat, [](auto...) { return get_dummy_attr("", SSH_FILEXFER_TYPE_REGULAR); });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.mkdir_recursive(nullptr, "some/nested/path"), mp::SFTPError,
                         mpt::match_what(StrEq("cannot overwrite remote non-directory \"some\" with directory")));
}

TEST_F(SFTPUtils, mkdir_cannot_create_dir)
{
    REPLACE(sftp_lstat, [](auto...) { return nullptr; });
    REPLACE(sftp_mkdir, [](auto...) { return -1; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });
    sftp_session_struct sftp{};

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.mkdir_recursive(&sftp, "some/nested/path"), mp::SFTPError,
        mpt::match_what(StrEq("cannot create remote directory \"some\": SFTP server: Permission denied")));
}

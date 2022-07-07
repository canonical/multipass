#include "common.h"
#include "mock_file_ops.h"
#include "mock_sftp.h"
#include "mock_ssh.h"

#include <multipass/ssh/sftp_utils.h>

namespace mp = multipass;
namespace mpt = multipass::test;

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

    EXPECT_EQ(MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), "target/path/path");
}

TEST_F(SFTPUtils, get_full_local_file_target__target_exists_not_dir)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(false));

    EXPECT_EQ(MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), "target/path");
}

TEST_F(SFTPUtils, get_full_local_file_target__target_not_exists_parent_does)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, exists(target_path.parent_path(), _)).WillOnce(Return(true));

    EXPECT_EQ(MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), "target/path");
}

TEST_F(SFTPUtils, get_full_local_file_target__target_not_exists_parent_neither)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, exists(target_path.parent_path(), _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), std::runtime_error,
                         mpt::match_what(StrEq("[sftp] local target does not exist")));
}

TEST_F(SFTPUtils, get_full_local_file_target__target_is_dir_child_is_too)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path / source_path.filename(), _)).WillOnce(Return(true));

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot overwrite local directory 'target/path/path' with non-directory")));
}

TEST_F(SFTPUtils, get_full_local_file_target__cannot_access_target)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return false;
    });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), std::runtime_error,
                         mpt::match_what(StrEq("[sftp] cannot access 'target/path': Permission denied")));
}

TEST_F(SFTPUtils, get_full_local_file_target__cannot_access_parent)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, exists(target_path.parent_path(), _)).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return false;
    });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), std::runtime_error,
                         mpt::match_what(StrEq("[sftp] cannot access 'target': Permission denied")));
}

TEST_F(SFTPUtils, get_full_local_file_target__cannot_access_child)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path / source_path.filename(), _))
        .WillOnce([](auto, std::error_code& err) {
            err = std::make_error_code(std::errc::permission_denied);
            return false;
        });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_full_local_file_target(source_path, target_path), std::runtime_error,
                         mpt::match_what(StrEq("[sftp] cannot access 'target/path/path': Permission denied")));
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

    EXPECT_EQ(MP_SFTPUTILS.get_full_remote_file_target(nullptr, source_path, target_path), "target/path/path");
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_exists_not_dir)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? get_dummy_attr(path, SSH_FILEXFER_TYPE_REGULAR) : nullptr;
    });

    EXPECT_EQ(MP_SFTPUTILS.get_full_remote_file_target(nullptr, source_path, target_path), "target/path");
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_not_exists_parent_does)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? nullptr : get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY);
    });

    EXPECT_EQ(MP_SFTPUTILS.get_full_remote_file_target(nullptr, source_path, target_path), "target/path");
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_not_exists_parent_neither)
{
    REPLACE(sftp_stat, [](auto...) { return nullptr; });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_full_remote_file_target(nullptr, source_path, target_path),
                         std::runtime_error, mpt::match_what(StrEq("[sftp] remote target does not exist")));
}

TEST_F(SFTPUtils, get_full_remote_file_target__target_is_dir_child_is_too)
{
    REPLACE(sftp_stat, [](auto, auto path) { return get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY); });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_remote_file_target(nullptr, source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot overwrite remote directory 'target/path/path' with non-directory")));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_exists_not_dir)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot overwrite local non-directory 'target/path' with directory")));
}

TEST_F(SFTPUtils, get_full_local_dir_target__cannot_access_target)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return false;
    });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), std::runtime_error,
                         mpt::match_what(StrEq("[sftp] cannot access 'target/path': Permission denied")));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_not_exists_can_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path, _)).WillOnce(Return(true));

    EXPECT_EQ(MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), "target/path");
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_not_exists_cannot_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path, _)).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return false;
    });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot create local directory 'target/path': Permission denied")));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_child_is_not)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path / source_path.filename(), _)).WillRepeatedly(Return(false));

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot overwrite local non-directory 'target/path/path' with directory")));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_child_not_exists_can_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _)).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path / source_path.filename(), _)).WillRepeatedly(Return(true));

    EXPECT_EQ(MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), "target/path/path");
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_child_not_exists_cannot_create)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _)).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_file_ops, create_directory(target_path / source_path.filename(), _))
        .WillOnce([](auto, std::error_code& err) {
            err = std::make_error_code(std::errc::permission_denied);
            return false;
        });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot create local directory 'target/path/path': Permission denied")));
}

TEST_F(SFTPUtils, get_full_local_dir_target__target_is_dir_cannot_access_child)
{
    EXPECT_CALL(*mock_file_ops, exists(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, is_directory(target_path, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, exists(target_path / source_path.filename(), _))
        .WillRepeatedly([](auto, std::error_code& err) {
            err = std::make_error_code(std::errc::permission_denied);
            return false;
        });

    MP_EXPECT_THROW_THAT(MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path), std::runtime_error,
                         mpt::match_what(StrEq("[sftp] cannot access 'target/path/path': Permission denied")));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_exists_not_dir)
{
    REPLACE(sftp_stat, [](auto, auto path) { return get_dummy_attr(path, SSH_FILEXFER_TYPE_REGULAR); });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_remote_dir_target(nullptr, source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot overwrite remote non-directory 'target/path' with directory")));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_not_exists_can_create)
{
    REPLACE(sftp_stat, [](auto...) { return nullptr; });
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    EXPECT_EQ(MP_SFTPUTILS.get_full_remote_dir_target(nullptr, source_path, target_path), "target/path");
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_not_exists_cannot_create)
{
    REPLACE(sftp_stat, [](auto...) { return nullptr; });
    REPLACE(sftp_mkdir, [](auto...) { return -1; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    sftp_session_struct sftp{};
    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_remote_dir_target(&sftp, source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot create remote directory 'target/path': SFTP server: Permission denied")));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_is_dir_child_is_not)
{
    REPLACE(sftp_stat, [&](auto, auto path) -> sftp_attributes {
        if (target_path == path)
            return get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY);
        if (target_path / source_path.filename() == path)
            return get_dummy_attr(path, SSH_FILEXFER_TYPE_REGULAR);
        return nullptr;
    });

    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_remote_dir_target(nullptr, source_path, target_path), std::runtime_error,
        mpt::match_what(StrEq("[sftp] cannot overwrite remote non-directory 'target/path/path' with directory")));
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_is_dir_child_not_exists_can_create)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY) : nullptr;
    });
    REPLACE(sftp_mkdir, [](auto...) { return SSH_FX_OK; });

    EXPECT_EQ(MP_SFTPUTILS.get_full_remote_dir_target(nullptr, source_path, target_path), "target/path/path");
}

TEST_F(SFTPUtils, get_full_remote_dir_target__target_is_dir_child_not_exists_cannot_create)
{
    REPLACE(sftp_stat, [&](auto, auto path) {
        return target_path == path ? get_dummy_attr(path, SSH_FILEXFER_TYPE_DIRECTORY) : nullptr;
    });
    REPLACE(sftp_mkdir, [](auto...) { return -1; });
    REPLACE(ssh_get_error, [](auto...) { return "SFTP server: Permission denied"; });

    sftp_session_struct sftp{};
    MP_EXPECT_THROW_THAT(
        MP_SFTPUTILS.get_full_remote_dir_target(&sftp, source_path, target_path), std::runtime_error,
        mpt::match_what(
            StrEq("[sftp] cannot create remote directory 'target/path/path': SFTP server: Permission denied")));
}

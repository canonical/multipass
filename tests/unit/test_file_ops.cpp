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

#include <multipass/file_ops.h>

#include <fcntl.h>

using namespace testing;
namespace fs = multipass::fs;
namespace mpt = multipass::test;

struct FileOps : public Test
{
    fs::path temp_dir = fs::temp_directory_path() / "multipass_fileops_test";
    fs::path temp_file = temp_dir / "file.txt";
    std::error_code err;
    std::string file_content = "content";

    FileOps()
    {
        fs::create_directory(temp_dir);
        std::ofstream stream{temp_file};
        stream << file_content;
    }

    ~FileOps()
    {
        fs::remove_all(temp_dir);
    }
};

TEST_F(FileOps, openWrite)
{
    auto file = MP_FILEOPS.open_write(temp_file);
    EXPECT_NE(dynamic_cast<std::ofstream*>(file.get()), nullptr);
}

TEST_F(FileOps, openRead)
{
    auto file = MP_FILEOPS.open_read(temp_file);
    EXPECT_NE(dynamic_cast<std::ifstream*>(file.get()), nullptr);
}

TEST_F(FileOps, exists)
{
    EXPECT_TRUE(MP_FILEOPS.exists(temp_dir, err));
    EXPECT_FALSE(err);
    EXPECT_FALSE(MP_FILEOPS.exists(temp_dir / "nonexistent", err));
    EXPECT_FALSE(err);
}

TEST_F(FileOps, copy)
{
    const fs::path src_dir = temp_dir / "sub_src_dir";
    const fs::path dest_dir = temp_dir / "sub_dest_dir";
    MP_FILEOPS.create_directory(src_dir, err);

    EXPECT_NO_THROW(MP_FILEOPS.copy(src_dir, dest_dir, std::filesystem::copy_options::recursive));
    EXPECT_TRUE(MP_FILEOPS.exists(dest_dir, err));
}

TEST_F(FileOps, isDirectory)
{
    EXPECT_TRUE(MP_FILEOPS.is_directory(temp_dir, err));
    EXPECT_FALSE(err);
    EXPECT_FALSE(MP_FILEOPS.is_directory(temp_file, err));
    EXPECT_FALSE(err);
}

TEST_F(FileOps, createDirectory)
{
    EXPECT_TRUE(MP_FILEOPS.create_directory(temp_dir / "subdir", err));
    EXPECT_FALSE(err);
    EXPECT_FALSE(MP_FILEOPS.create_directory(temp_dir / "subdir", err));
    EXPECT_FALSE(err);
}

TEST_F(FileOps, remove)
{
    EXPECT_TRUE(MP_FILEOPS.remove(temp_file, err));
    EXPECT_FALSE(err);
    EXPECT_FALSE(MP_FILEOPS.remove(temp_file, err));
    EXPECT_FALSE(err);
}

TEST_F(FileOps, symlink)
{
    MP_FILEOPS.create_symlink(temp_file, temp_dir / "symlink", err);
    EXPECT_FALSE(err);
    MP_FILEOPS.create_symlink(temp_file, temp_dir / "symlink", err);
    EXPECT_TRUE(err);
    EXPECT_EQ(MP_FILEOPS.read_symlink(temp_dir / "symlink", err), temp_file);
    EXPECT_FALSE(err);
}

TEST_F(FileOps, status)
{
    auto dir_status = MP_FILEOPS.status(temp_dir, err);
    EXPECT_NE(dir_status.permissions(), fs::perms::unknown);
    EXPECT_EQ(dir_status.type(), fs::file_type::directory);
    EXPECT_FALSE(err);
}

TEST_F(FileOps, recurisveDirIter)
{
    auto iter = MP_FILEOPS.recursive_dir_iterator(temp_dir, err);
    EXPECT_FALSE(err);
    EXPECT_TRUE(iter->hasNext());
    EXPECT_EQ(iter->next().path(), temp_file);
    MP_FILEOPS.recursive_dir_iterator(temp_file, err);
    EXPECT_TRUE(err);
}

TEST_F(FileOps, createDirectories)
{
    EXPECT_TRUE(MP_FILEOPS.create_directories(temp_dir / "subdir/nested", err));
    EXPECT_FALSE(err);
    EXPECT_FALSE(MP_FILEOPS.create_directories(temp_dir / "subdir/nested", err));
    EXPECT_FALSE(err);
}

TEST_F(FileOps, dirIter)
{
    auto iter = MP_FILEOPS.dir_iterator(temp_dir, err);
    EXPECT_FALSE(err);
    EXPECT_TRUE(iter->hasNext());
    EXPECT_EQ(iter->next().path(), temp_dir / ".");
    EXPECT_EQ(iter->next().path(), temp_dir / "..");
    EXPECT_EQ(iter->next().path(), temp_file);
    MP_FILEOPS.recursive_dir_iterator(temp_file, err);
    EXPECT_TRUE(err);
}

TEST_F(FileOps, posixOpen)
{
    const auto named_fd1 = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    EXPECT_NE(named_fd1->fd, -1);
    const auto named_fd2 = MP_FILEOPS.open_fd(temp_dir, O_RDWR, 0);
    EXPECT_EQ(named_fd2->fd, -1);
}

TEST_F(FileOps, posixRead)
{
    const auto named_fd = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    std::array<char, 100> buffer{};
    const auto r = MP_FILEOPS.read(named_fd->fd, buffer.data(), buffer.size());
    EXPECT_EQ(r, file_content.size());
    EXPECT_EQ(buffer.data(), file_content);
}

TEST_F(FileOps, posixWrite)
{
    const auto named_fd = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    const char data[] = "abcdef";
    const auto r = MP_FILEOPS.write(named_fd->fd, data, sizeof(data));
    EXPECT_EQ(r, sizeof(data));
    std::ifstream stream{temp_file};
    std::string string{std::istreambuf_iterator{stream}, {}};
    EXPECT_STREQ(string.c_str(), data);
}

TEST_F(FileOps, posixLseek)
{
    const auto named_fd = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    const auto seek = 3;
    MP_FILEOPS.lseek(named_fd->fd, seek, SEEK_SET);
    std::array<char, 100> buffer{};
    const auto r = MP_FILEOPS.read(named_fd->fd, buffer.data(), buffer.size());
    EXPECT_EQ(r, file_content.size() - seek);
    EXPECT_STREQ(buffer.data(), file_content.c_str() + seek);
}

TEST_F(FileOps, removeExtension)
{
    EXPECT_EQ(MP_FILEOPS.remove_extension(""), "");
    EXPECT_EQ(MP_FILEOPS.remove_extension("test"), "test");
    EXPECT_EQ(MP_FILEOPS.remove_extension(".empty"), ".empty");
    EXPECT_EQ(MP_FILEOPS.remove_extension("tests/.empty"), "tests/.empty");

    EXPECT_EQ(MP_FILEOPS.remove_extension("test.txt"), "test");
    EXPECT_EQ(MP_FILEOPS.remove_extension("tests/.empty.txt"), "tests/.empty");
    EXPECT_EQ(MP_FILEOPS.remove_extension("tests/test.test.txt"), "tests/test.test");
    EXPECT_EQ(MP_FILEOPS.remove_extension("tests/bar.foo.tar.gz"), "tests/bar.foo.tar");
    EXPECT_EQ(MP_FILEOPS.remove_extension("/sets/test.png"), "/sets/test");
}

struct HighLevelFileOps : public Test
{
    HighLevelFileOps()
    {
        // Call the real high-level functions
        EXPECT_CALL(mock_file_ops, write_transactionally)
            .WillRepeatedly([this]<typename... Args>(Args&&... args) {
                return mock_file_ops.FileOps::write_transactionally(std::forward<Args>(args)...);
            });
        EXPECT_CALL(mock_file_ops, try_read_file(A<const fs::path&>()))
            .WillRepeatedly([this]<typename... Args>(Args&&... args) {
                return mock_file_ops.FileOps::try_read_file(std::forward<Args>(args)...);
            });
    }

    mpt::MockFileOps::GuardedMock guarded_mock_file_ops = mpt::MockFileOps::inject<StrictMock>();
    mpt::MockFileOps& mock_file_ops = *guarded_mock_file_ops.first;
    inline static const QString dir = QStringLiteral("a/b/c");
    inline static const QString file_name = QStringLiteral("asd.blag");
    inline static const QString file_path = QStringLiteral("%1/%2").arg(dir, file_name);
    inline static const QString lockfile_path = file_path + ".lock";
    inline static const char* file_text = R"({"a": [1,2,3]})";
    inline static const auto expected_stale_lock_time = std::chrono::seconds{10};
    inline static const auto expected_lock_timeout = std::chrono::seconds{10};
    inline static const auto expected_retry_attempts = 10;
};

TEST_F(HighLevelFileOps, writesTransactionally)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)),
                                 Eq(expected_stale_lock_time)));
    EXPECT_CALL(
        mock_file_ops,
        tryLock(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));

    EXPECT_CALL(mock_file_ops, mkpath(Eq(dir), Eq("."))).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(mpt::FileNameMatches(Eq(file_path)), _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops,
                write(mpt::FileNameMatches(Eq(file_path)), Eq(file_text), Eq(strlen(file_text))))
        .WillOnce(Return(14));
    EXPECT_CALL(mock_file_ops, commit(mpt::FileNameMatches<QSaveFile&>(Eq(file_path))))
        .WillOnce(Return(true));
    EXPECT_NO_THROW(mock_file_ops.write_transactionally(file_path, file_text));
}

TEST_F(HighLevelFileOps, writesTransactionallyEventually)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)),
                                 Eq(expected_stale_lock_time)));
    EXPECT_CALL(
        mock_file_ops,
        tryLock(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));

    EXPECT_CALL(mock_file_ops, mkpath(Eq(dir), Eq("."))).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(mpt::FileNameMatches(Eq(file_path)), _))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_file_ops,
                write(mpt::FileNameMatches(Eq(file_path)), Eq(file_text), Eq(strlen(file_text))))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(14));

    auto commit_called_times = 0;
    EXPECT_CALL(mock_file_ops, commit(mpt::FileNameMatches<QSaveFile&>(Eq(file_path))))
        .Times(expected_retry_attempts)
        .WillRepeatedly(
            InvokeWithoutArgs([&]() { return ++commit_called_times == expected_retry_attempts; }));

    EXPECT_NO_THROW(mock_file_ops.write_transactionally(file_path, file_text));
}

TEST_F(HighLevelFileOps, writeTransactionallyThrowsOnFailureToCreateDirectory)
{
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(
        mock_file_ops.write_transactionally(file_path, file_text),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not create"), HasSubstr(dir.toStdString()))));
}

TEST_F(HighLevelFileOps, writeTransactionallyThrowsOnFailureToOpenFile)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)),
                                 Eq(expected_stale_lock_time)));
    EXPECT_CALL(
        mock_file_ops,
        tryLock(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _)).WillOnce(Return(false));
    MP_EXPECT_THROW_THAT(
        mock_file_ops.write_transactionally(file_path, file_text),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not open"), HasSubstr(file_path.toStdString()))));
}

TEST_F(HighLevelFileOps, writeTransactionallyThrowsOnFailureToWriteFile)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)),
                                 Eq(expected_stale_lock_time)));
    EXPECT_CALL(
        mock_file_ops,
        tryLock(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, write(A<QIODevice&>(), _, _)).WillOnce(Return(-1));
    MP_EXPECT_THROW_THAT(
        mock_file_ops.write_transactionally(file_path, file_text),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not write"), HasSubstr(file_path.toStdString()))));
}

TEST_F(HighLevelFileOps, writeTransactionallyThrowsOnFailureToAcquireLock)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)),
                                 Eq(expected_stale_lock_time)));
    EXPECT_CALL(
        mock_file_ops,
        tryLock(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)), Eq(expected_lock_timeout)))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));

    MP_EXPECT_THROW_THAT(mock_file_ops.write_transactionally(file_path, file_text),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Could not acquire lock"),
                                               HasSubstr(file_path.toStdString()))));
}

TEST_F(HighLevelFileOps, writeTransactionallyThrowsOnFailureToCommit)
{
    EXPECT_CALL(mock_file_ops,
                setStaleLockTime(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)),
                                 Eq(expected_stale_lock_time)));
    EXPECT_CALL(
        mock_file_ops,
        tryLock(mpt::FileNameMatches<QLockFile&>(Eq(lockfile_path)), Eq(expected_lock_timeout)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, mkpath).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open(_, _))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_file_ops, write(A<QIODevice&>(), _, _))
        .Times(expected_retry_attempts)
        .WillRepeatedly(Return(1234));
    EXPECT_CALL(mock_file_ops, commit).Times(expected_retry_attempts).WillRepeatedly(Return(false));
    MP_EXPECT_THROW_THAT(
        mock_file_ops.write_transactionally(file_path, file_text),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Could not commit"), HasSubstr(file_path.toStdString()))));
}

TEST_F(HighLevelFileOps, tryReadFileReadsFromFile)
{
    auto filestream = std::make_unique<std::stringstream>();
    *filestream << "Hello, world!";

    EXPECT_CALL(mock_file_ops, exists(_, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open_read(_, _)).WillOnce(Return(std::move(filestream)));
    const auto filedata = mock_file_ops.try_read_file("exists");

    EXPECT_EQ(*filedata, "Hello, world!");
}

TEST_F(HighLevelFileOps, tryReadFileReturnsNulloptForMissingFile)
{
    EXPECT_CALL(mock_file_ops, exists(_, _)).WillOnce(Return(false));
    const auto filedata = mock_file_ops.try_read_file("exists");

    EXPECT_EQ(filedata, std::nullopt);
}

TEST_F(HighLevelFileOps, tryReadFileThrowsOnExistsErr)
{
    EXPECT_CALL(mock_file_ops, exists(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(std::error_code(EACCES, std::system_category())),
                        Return(false)));

    EXPECT_THROW(mock_file_ops.try_read_file(":("), std::filesystem::filesystem_error);
}

TEST_F(HighLevelFileOps, tryReadFileThrowsOnFailbit)
{
    auto filestream = std::make_unique<std::stringstream>();
    filestream->setstate(std::ios_base::failbit);

    EXPECT_CALL(mock_file_ops, exists(_, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open_read(_, _)).WillOnce(Return(std::move(filestream)));

    EXPECT_THROW(mock_file_ops.try_read_file(":("), std::ios_base::failure);
}

TEST_F(HighLevelFileOps, tryReadFileThrowsOnBadbit)
{
    auto filestream = std::make_unique<std::stringstream>();
    filestream->setstate(std::ios_base::badbit);

    EXPECT_CALL(mock_file_ops, exists(_, _)).WillOnce(Return(true));
    EXPECT_CALL(mock_file_ops, open_read(_, _)).WillOnce(Return(std::move(filestream)));

    EXPECT_THROW(mock_file_ops.try_read_file(":("), std::ios_base::failure);
}

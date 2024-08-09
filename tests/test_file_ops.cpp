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

#include <multipass/file_ops.h>

#include <fcntl.h>

using namespace testing;
namespace fs = multipass::fs;

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

TEST_F(FileOps, open_write)
{
    auto file = MP_FILEOPS.open_write(temp_file);
    EXPECT_NE(dynamic_cast<std::ofstream*>(file.get()), nullptr);
}

TEST_F(FileOps, open_read)
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

TEST_F(FileOps, is_directory)
{
    EXPECT_TRUE(MP_FILEOPS.is_directory(temp_dir, err));
    EXPECT_FALSE(err);
    EXPECT_FALSE(MP_FILEOPS.is_directory(temp_file, err));
    EXPECT_FALSE(err);
}

TEST_F(FileOps, create_directory)
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

TEST_F(FileOps, permissions)
{
    MP_FILEOPS.permissions(temp_file, fs::perms::all, err);
    EXPECT_FALSE(err);
    MP_FILEOPS.permissions(temp_dir / "nonexistent", fs::perms::all, err);
    EXPECT_TRUE(err);
}

TEST_F(FileOps, status)
{
    auto dir_status = MP_FILEOPS.status(temp_dir, err);
    EXPECT_NE(dir_status.permissions(), fs::perms::unknown);
    EXPECT_EQ(dir_status.type(), fs::file_type::directory);
    EXPECT_FALSE(err);
}

TEST_F(FileOps, recurisve_dir_iter)
{
    auto iter = MP_FILEOPS.recursive_dir_iterator(temp_dir, err);
    EXPECT_FALSE(err);
    EXPECT_TRUE(iter->hasNext());
    EXPECT_EQ(iter->next().path(), temp_file);
    MP_FILEOPS.recursive_dir_iterator(temp_file, err);
    EXPECT_TRUE(err);
}

TEST_F(FileOps, create_directories)
{
    EXPECT_TRUE(MP_FILEOPS.create_directories(temp_dir / "subdir/nested", err));
    EXPECT_FALSE(err);
    EXPECT_FALSE(MP_FILEOPS.create_directories(temp_dir / "subdir/nested", err));
    EXPECT_FALSE(err);
}

TEST_F(FileOps, dir_iter)
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

TEST_F(FileOps, posix_open)
{
    const auto named_fd1 = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    EXPECT_NE(named_fd1->fd, -1);
    const auto named_fd2 = MP_FILEOPS.open_fd(temp_dir, O_RDWR, 0);
    EXPECT_EQ(named_fd2->fd, -1);
}

TEST_F(FileOps, posix_read)
{
    const auto named_fd = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    std::array<char, 100> buffer{};
    const auto r = MP_FILEOPS.read(named_fd->fd, buffer.data(), buffer.size());
    EXPECT_EQ(r, file_content.size());
    EXPECT_EQ(buffer.data(), file_content);
}

TEST_F(FileOps, posix_write)
{
    const auto named_fd = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    const char data[] = "abcdef";
    const auto r = MP_FILEOPS.write(named_fd->fd, data, sizeof(data));
    EXPECT_EQ(r, sizeof(data));
    std::ifstream stream{temp_file};
    std::string string{std::istreambuf_iterator{stream}, {}};
    EXPECT_STREQ(string.c_str(), data);
}

TEST_F(FileOps, posix_lseek)
{
    const auto named_fd = MP_FILEOPS.open_fd(temp_file, O_RDWR, 0);
    const auto seek = 3;
    MP_FILEOPS.lseek(named_fd->fd, seek, SEEK_SET);
    std::array<char, 100> buffer{};
    const auto r = MP_FILEOPS.read(named_fd->fd, buffer.data(), buffer.size());
    EXPECT_EQ(r, file_content.size() - seek);
    EXPECT_STREQ(buffer.data(), file_content.c_str() + seek);
}

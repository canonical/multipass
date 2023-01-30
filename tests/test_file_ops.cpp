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

using namespace testing;
namespace fs = multipass::fs;

struct FileOps : public Test
{
    fs::path temp_dir = fs::temp_directory_path() / "multipass_fileops_test";
    fs::path temp_file = temp_dir / "file.txt";
    std::error_code err;

    FileOps()
    {
        fs::create_directory(temp_dir);
        std::ofstream{temp_file};
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

TEST_F(FileOps, dir_iter)
{
    MP_FILEOPS.recursive_dir_iterator(temp_dir, err);
    EXPECT_FALSE(err);
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

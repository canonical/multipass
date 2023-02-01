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

#include <chrono>
#include <fstream>
#include <multipass/recursive_dir_iterator.h>

namespace mp = multipass;
namespace fs = mp::fs;

using namespace testing;

struct RecursiveDirIterator : public Test
{
    fs::path temp_dir = fs::temp_directory_path() / "multipass_recursive_dir_iter_test";
    fs::path temp_file = temp_dir / "file.txt";
    std::error_code err;
    mp::DirectoryEntry entry;

    RecursiveDirIterator()
    {
        fs::create_directory(temp_dir);
        std::ofstream{temp_file};
    }

    ~RecursiveDirIterator()
    {
        fs::remove_all(temp_dir);
    }
};

TEST_F(RecursiveDirIterator, assign)
{
    EXPECT_NO_THROW(entry.assign(temp_dir));
    entry.assign(temp_dir, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(entry.path(), temp_dir);
}

TEST_F(RecursiveDirIterator, replace_filename)
{
    entry.assign(temp_dir / "filename");
    EXPECT_NO_THROW(entry.replace_filename(temp_file.filename()));
    entry.replace_filename(temp_file.filename(), err);
    EXPECT_FALSE(err);
    EXPECT_EQ(entry.path().filename(), temp_file.filename());
}

TEST_F(RecursiveDirIterator, refresh)
{
    entry.assign(temp_dir);
    EXPECT_NO_THROW(entry.refresh());
    entry.refresh(err);
    EXPECT_FALSE(err);
}

TEST_F(RecursiveDirIterator, exists)
{
    entry.assign(temp_dir);
    EXPECT_NO_THROW(entry.exists());
    EXPECT_TRUE(entry.exists());
    EXPECT_TRUE(entry.exists(err));
    EXPECT_FALSE(err);

    entry.assign(temp_dir / "nonexistent");
    EXPECT_NO_THROW(entry.exists());
    EXPECT_FALSE(entry.exists());
    EXPECT_FALSE(entry.exists(err));
}

TEST_F(RecursiveDirIterator, is_type)
{
    entry.assign(temp_file);

    EXPECT_NO_THROW(entry.is_regular_file());
    EXPECT_TRUE(entry.is_regular_file());
    EXPECT_TRUE(entry.is_regular_file(err));
    EXPECT_FALSE(err);

#define IS_NOT_TYPE(type)                                                                                              \
    EXPECT_NO_THROW(entry.is_##type());                                                                                \
    EXPECT_FALSE(entry.is_##type());                                                                                   \
    EXPECT_FALSE(entry.is_##type(err));                                                                                \
    EXPECT_FALSE(err);

    IS_NOT_TYPE(block_file);
    IS_NOT_TYPE(character_file);
    IS_NOT_TYPE(directory);
    IS_NOT_TYPE(fifo);
    IS_NOT_TYPE(other);
    IS_NOT_TYPE(socket);
    IS_NOT_TYPE(symlink);
}

TEST_F(RecursiveDirIterator, file_size)
{
    entry.assign(temp_file);
    EXPECT_NO_THROW(entry.file_size());
    EXPECT_EQ(entry.file_size(), 0);
    EXPECT_EQ(entry.file_size(err), 0);
    EXPECT_FALSE(err);
}

TEST_F(RecursiveDirIterator, hard_link_count)
{
    entry.assign(temp_file);
    EXPECT_NO_THROW(entry.file_size());
    EXPECT_EQ(entry.hard_link_count(), 1);
    EXPECT_EQ(entry.hard_link_count(err), 1);
    EXPECT_FALSE(err);
}

TEST_F(RecursiveDirIterator, last_write_time)
{
    entry.assign(temp_file);
    auto file_date = entry.last_write_time();
    entry.assign(temp_dir);
    auto dir_date = entry.last_write_time(err);
    EXPECT_LE(file_date, dir_date);
    EXPECT_FALSE(err);
}

TEST_F(RecursiveDirIterator, status)
{
    entry.assign(temp_dir);
    EXPECT_NO_THROW(entry.status());
    EXPECT_NO_THROW(entry.symlink_status());

    auto s = entry.status(err);
    EXPECT_FALSE(err);
    auto ss = entry.symlink_status(err);
    EXPECT_FALSE(err);

    EXPECT_EQ(s.type(), fs::file_type::directory);
    EXPECT_EQ(ss.type(), fs::file_type::directory);

    EXPECT_NE(s.permissions(), fs::perms::unknown);
    EXPECT_NE(ss.permissions(), fs::perms::unknown);
}

TEST_F(RecursiveDirIterator, equal)
{
    mp::DirectoryEntry dir_entry{};
    dir_entry.assign(temp_dir);
    mp::DirectoryEntry file_entry{};
    dir_entry.assign(temp_file);

    EXPECT_FALSE(dir_entry == file_entry);
}

TEST_F(RecursiveDirIterator, hasNext)
{
    mp::RecursiveDirIterator iter{temp_dir, err};
    EXPECT_FALSE(err);
    EXPECT_TRUE(iter.hasNext());
}

TEST_F(RecursiveDirIterator, next)
{
    mp::RecursiveDirIterator iter{temp_dir, err};
    EXPECT_FALSE(err);
    EXPECT_EQ(fs::canonical(iter.next().path()), fs::canonical(temp_file));
}

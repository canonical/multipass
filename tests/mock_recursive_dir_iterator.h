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

#ifndef MULTIPASS_MOCK_STD_RECURSIVE_DIR_ITER_H
#define MULTIPASS_MOCK_STD_RECURSIVE_DIR_ITER_H

#include "common.h"
#include <multipass/recursive_dir_iterator.h>

namespace multipass::test
{
struct MockDirectoryEntry : public DirectoryEntry
{
    MOCK_METHOD(void, assign, (const fs::path& path), (override));
    MOCK_METHOD(void, assign, (const fs::path& path, std::error_code& err), (override));
    MOCK_METHOD(void, replace_filename, (const fs::path& path), (override));
    MOCK_METHOD(void, replace_filename, (const fs::path& path, std::error_code& err), (override));
    MOCK_METHOD(void, refresh, (), (override));
    MOCK_METHOD(void, refresh, (std::error_code & err), (noexcept, override));
    MOCK_METHOD(const fs::path&, path, (), (const, noexcept, override));
    MOCK_METHOD(bool, exists, (), (const, override));
    MOCK_METHOD(bool, exists, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_block_file, (), (const, override));
    MOCK_METHOD(bool, is_block_file, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_character_file, (), (const, override));
    MOCK_METHOD(bool, is_character_file, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_directory, (), (const, override));
    MOCK_METHOD(bool, is_directory, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_fifo, (), (const, override));
    MOCK_METHOD(bool, is_fifo, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_other, (), (const, override));
    MOCK_METHOD(bool, is_other, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_regular_file, (), (const, override));
    MOCK_METHOD(bool, is_regular_file, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_socket, (), (const, override));
    MOCK_METHOD(bool, is_socket, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(bool, is_symlink, (), (const, override));
    MOCK_METHOD(bool, is_symlink, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(uintmax_t, file_size, (), (const, override));
    MOCK_METHOD(uintmax_t, file_size, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(uintmax_t, hard_link_count, (), (const, override));
    MOCK_METHOD(uintmax_t, hard_link_count, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(fs::file_time_type, last_write_time, (), (const, override));
    MOCK_METHOD(fs::file_time_type, last_write_time, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(fs::file_status, status, (), (const, override));
    MOCK_METHOD(fs::file_status, status, (std::error_code & err), (const, noexcept, override));
    MOCK_METHOD(fs::file_status, symlink_status, (), (const, override));
    MOCK_METHOD(fs::file_status, symlink_status, (std::error_code & err), (const, noexcept, override));
};

struct MockRecursiveDirIterator : public RecursiveDirIterator
{
    MOCK_METHOD(bool, hasNext, (), (override));
    MOCK_METHOD(const DirectoryEntry&, next, (), (override));
};

struct MockDirIterator : public DirIterator
{
    MOCK_METHOD(bool, hasNext, (), (override));
    MOCK_METHOD(const DirectoryEntry&, next, (), (override));
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_STD_RECURSIVE_DIR_ITER_H

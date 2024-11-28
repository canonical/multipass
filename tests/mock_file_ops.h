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

#ifndef MULTIPASS_MOCK_CONST_FILE_OPS_H
#define MULTIPASS_MOCK_CONST_FILE_OPS_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/file_ops.h>

namespace multipass::test
{
class MockFileOps : public FileOps
{
public:
    using FileOps::FileOps;

    // QDir mock methods
    MOCK_METHOD(QDir, current, (), (const));
    MOCK_METHOD(bool, exists, (const QDir&), (const, override));
    MOCK_METHOD(bool, isReadable, (const QDir&), (const, override));
    MOCK_METHOD(bool, mkpath, (const QDir&, const QString& dirName), (const, override));
    MOCK_METHOD(bool, rmdir, (QDir&, const QString& dirName), (const, override));

    // QFileInfo mock methods
    MOCK_METHOD(bool, exists, (const QFileInfo&), (const, override));
    MOCK_METHOD(bool, isDir, (const QFileInfo&), (const, override));
    MOCK_METHOD(bool, isReadable, (const QFileInfo&), (const, override));
    MOCK_METHOD(uint, ownerId, (const QFileInfo&), (const, override));
    MOCK_METHOD(uint, groupId, (const QFileInfo&), (const, override));

    // QFile mock methods
    MOCK_METHOD(bool, exists, (const QFile&), (const, override));
    MOCK_METHOD(bool, is_open, (const QFile&), (const, override));
    MOCK_METHOD(bool, open, (QFileDevice&, QIODevice::OpenMode), (const, override));
    MOCK_METHOD(QFileDevice::Permissions, permissions, (const QFile&), (const, override));
    MOCK_METHOD(qint64, read, (QFile&, char*, qint64), (const, override));
    MOCK_METHOD(QByteArray, read_all, (QFile&), (const, override));
    MOCK_METHOD(QString, read_line, (QTextStream&), (const, override));
    MOCK_METHOD(bool, remove, (QFile&), (const, override));
    MOCK_METHOD(bool, rename, (QFile&, const QString& newName), (const, override));
    MOCK_METHOD(bool, resize, (QFile&, qint64 sz), (const, override));
    MOCK_METHOD(bool, seek, (QFile&, qint64 pos), (const, override));
    MOCK_METHOD(bool, setPermissions, (QFile&, QFileDevice::Permissions), (const, override));
    MOCK_METHOD(qint64, size, (QFile&), (const, override));
    MOCK_METHOD(qint64, write, (QFile&, const char*, qint64), (const, override));
    MOCK_METHOD(qint64, write, (QFileDevice&, const QByteArray&), (const, override));
    MOCK_METHOD(bool, flush, (QFile & file), (const, override));

    // QSaveFile mock methods
    MOCK_METHOD(bool, commit, (QSaveFile&), (const, override));

    // posix mock methods
    MOCK_METHOD((std::unique_ptr<NamedFd>), open_fd, (const fs::path&, int, int), (const, override));
    MOCK_METHOD(int, read, (int, void*, size_t), (const, override));
    MOCK_METHOD(int, write, (int, const void*, size_t), (const, override));
    MOCK_METHOD(off_t, lseek, (int, off_t, int), (const, override));

    // Mock std methods
    MOCK_METHOD(void, open, (std::fstream&, const char*, std::ios_base::openmode), (const, override));
    MOCK_METHOD(bool, is_open, (const std::ifstream&), (const, override));
    MOCK_METHOD(std::ifstream&, read, (std::ifstream&, char*, std::streamsize), (const, override));
    MOCK_METHOD(std::unique_ptr<std::ostream>, open_write, (const fs::path& path, std::ios_base::openmode mode),
                (override, const));
    MOCK_METHOD(std::unique_ptr<std::istream>, open_read, (const fs::path& path, std::ios_base::openmode mode),
                (override, const));
    MOCK_METHOD(bool, exists, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(bool, is_directory, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(bool, create_directory, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(bool, create_directories, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(bool, remove, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(void, create_symlink, (const fs::path& to, const fs::path& path, std::error_code& err),
                (override, const));
    MOCK_METHOD(fs::path, read_symlink, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(void, permissions, (const fs::path& path, fs::perms perms, std::error_code& err), (override, const));
    MOCK_METHOD(fs::file_status, status, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(fs::file_status, symlink_status, (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(std::unique_ptr<multipass::RecursiveDirIterator>, recursive_dir_iterator,
                (const fs::path& path, std::error_code& err), (override, const));
    MOCK_METHOD(std::unique_ptr<multipass::DirIterator>,
                dir_iterator,
                (const fs::path& path, std::error_code& err),
                (override, const));
    MOCK_METHOD(fs::path, weakly_canonical, (const fs::path& path), (override, const));

    MP_MOCK_SINGLETON_BOILERPLATE(MockFileOps, FileOps);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_FILE_OPS_H

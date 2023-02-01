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
    MOCK_CONST_METHOD0(current, QDir());
    MOCK_CONST_METHOD1(exists, bool(const QDir&));
    MOCK_CONST_METHOD1(isReadable, bool(const QDir&));
    MOCK_CONST_METHOD2(mkpath, bool(const QDir&, const QString& dirName));
    MOCK_CONST_METHOD2(rmdir, bool(QDir&, const QString& dirName));

    // QFileInfo mock methods
    MOCK_CONST_METHOD1(exists, bool(const QFileInfo&));
    MOCK_CONST_METHOD1(isDir, bool(const QFileInfo&));
    MOCK_CONST_METHOD1(isReadable, bool(const QFileInfo&));

    // QFile mock methods
    MOCK_CONST_METHOD1(exists, bool(const QFile&));
    MOCK_CONST_METHOD1(is_open, bool(const QFile&));
    MOCK_CONST_METHOD2(open, bool(QFileDevice&, QIODevice::OpenMode));
    MOCK_CONST_METHOD1(permissions, QFileDevice::Permissions(const QFile&));
    MOCK_CONST_METHOD3(read, qint64(QFile&, char*, qint64));
    MOCK_CONST_METHOD1(read_all, QByteArray(QFile&));
    MOCK_CONST_METHOD1(read_line, QString(QTextStream&));
    MOCK_CONST_METHOD1(remove, bool(QFile&));
    MOCK_CONST_METHOD2(rename, bool(QFile&, const QString& newName));
    MOCK_CONST_METHOD2(resize, bool(QFile&, qint64 sz));
    MOCK_CONST_METHOD2(seek, bool(QFile&, qint64 pos));
    MOCK_CONST_METHOD2(setPermissions, bool(QFile&, QFileDevice::Permissions));
    MOCK_CONST_METHOD1(size, qint64(QFile&));
    MOCK_CONST_METHOD3(write, qint64(QFile&, const char*, qint64));
    MOCK_CONST_METHOD2(write, qint64(QFileDevice&, const QByteArray&));

    // QSaveFile mock methods
    MOCK_CONST_METHOD1(commit, bool(QSaveFile&));

    // Mock std methods
    MOCK_CONST_METHOD3(open, void(std::fstream&, const char*, std::ios_base::openmode));
    MOCK_METHOD(std::unique_ptr<std::ostream>, open_write, (const fs::path& path), (override, const));
    MOCK_METHOD(std::unique_ptr<std::istream>, open_read, (const fs::path& path), (override, const));
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
    MOCK_METHOD(std::unique_ptr<multipass::RecursiveDirIterator>, recursive_dir_iterator,
                (const fs::path& path, std::error_code& err), (override, const));

    MP_MOCK_SINGLETON_BOILERPLATE(MockFileOps, FileOps);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_FILE_OPS_H

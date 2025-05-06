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

#pragma once

#include "recursive_dir_iterator.h"
#include "singleton.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QDir>
#include <QFileDevice>
#include <QFileInfoList>
#include <QLockFile>
#include <QSaveFile>
#include <QString>
#include <QTextStream>

#include <filesystem>
#include <fstream>

#define MP_FILEOPS multipass::FileOps::instance()

namespace multipass
{
namespace fs = std::filesystem;

struct NamedFd
{
    NamedFd(const fs::path& path, int fd);
    ~NamedFd();

    fs::path path;
    int fd;
};

class FileOps : public Singleton<FileOps>
{
public:
    FileOps(const Singleton<FileOps>::PrivatePass&) noexcept;

    // High-level operations
    virtual void write_transactionally(const QString& file_name, const QByteArrayView& data) const;
    virtual std::optional<std::string> try_read_file(const fs::path& filename) const;

    // QDir operations
    virtual bool exists(const QDir& dir) const;
    virtual bool isReadable(const QDir& dir) const;
    virtual QFileInfoList entryInfoList(const QDir& dir,
                                        const QStringList& nameFilters,
                                        QDir::Filters filters = QDir::NoFilter,
                                        QDir::SortFlags sort = QDir::NoSort) const;
    virtual bool mkpath(const QDir& dir, const QString& dirName) const;
    virtual bool rmdir(QDir& dir, const QString& dirName) const;

    // QFileInfo operations
    virtual bool exists(const QFileInfo& file) const;
    virtual bool isDir(const QFileInfo& file) const;
    virtual bool isReadable(const QFileInfo& file) const;
    virtual uint ownerId(const QFileInfo& file) const;
    virtual uint groupId(const QFileInfo& file) const;

    // QFile (and parent classes) operations
    virtual bool exists(const QFile& file) const;
    virtual bool is_open(const QIODevice& file) const;
    virtual bool open(QIODevice& file, QIODevice::OpenMode mode) const;
    virtual qint64 read(QIODevice& file, char* data, qint64 maxSize) const;
    virtual QByteArray read_all(QIODevice& file) const;
    virtual bool remove(QFile& file) const;
    virtual bool rename(QFile& file, const QString& newName) const;
    virtual bool resize(QFileDevice& file, qint64 sz) const;
    virtual bool seek(QIODevice& file, qint64 pos) const;
    virtual qint64 size(QIODevice& file) const;
    virtual qint64 write(QIODevice& file, const char* data, qint64 maxSize) const;
    virtual qint64 write(QIODevice& file, const QByteArray& data) const;
    virtual bool flush(QFileDevice& file) const;

    virtual QString read_line(QTextStream& text_stream) const;

    virtual bool copy(const QString& from, const QString& to) const;

    // QSaveFile operations
    virtual bool commit(QSaveFile& file) const;

    // QLockFile operations
    virtual void setStaleLockTime(QLockFile& lock, std::chrono::milliseconds time) const;
    virtual bool tryLock(QLockFile& lock, std::chrono::milliseconds timeout) const;

    // posix operations
    virtual std::unique_ptr<NamedFd> open_fd(const fs::path& path, int flags, int perms) const;
    virtual int read(int fd, void* buf, size_t nbytes) const;
    virtual int write(int fd, const void* buf, size_t nbytes) const;
    virtual off_t lseek(int fd, off_t offset, int whence) const;

    // std operations
    virtual void open(std::fstream& stream,
                      const char* filename,
                      std::ios_base::openmode mode) const;
    virtual bool is_open(const std::ifstream& file) const;
    virtual std::ifstream& read(std::ifstream& file, char* buffer, std::streamsize size) const;
    virtual std::unique_ptr<std::ostream>
    open_write(const fs::path& path, std::ios_base::openmode mode = std::ios_base::out) const;
    virtual std::unique_ptr<std::istream>
    open_read(const fs::path& path, std::ios_base::openmode mode = std::ios_base::in) const;
    virtual void copy(const fs::path& src,
                      const fs::path& dist,
                      fs::copy_options copy_options) const;
    virtual void rename(const fs::path& old_p, const fs::path& new_p) const;
    virtual void rename(const fs::path& old_p,
                        const fs::path& new_p,
                        std::error_code& ec) const noexcept;
    virtual bool exists(const fs::path& path) const;
    virtual bool exists(const fs::path& path, std::error_code& err) const noexcept;
    virtual bool is_directory(const fs::path& path, std::error_code& err) const;
    virtual bool create_directory(const fs::path& path, std::error_code& err) const;
    virtual bool create_directories(const fs::path& path, std::error_code& err) const;
    virtual bool remove(const fs::path& path) const;
    virtual bool remove(const fs::path& path, std::error_code& err) const noexcept;
    virtual void create_symlink(const fs::path& to,
                                const fs::path& path,
                                std::error_code& err) const;
    virtual fs::path read_symlink(const fs::path& path, std::error_code& err) const;
    virtual fs::file_status status(const fs::path& path, std::error_code& err) const;
    virtual fs::file_status symlink_status(const fs::path& path, std::error_code& err) const;
    virtual std::unique_ptr<RecursiveDirIterator>
    recursive_dir_iterator(const fs::path& path, std::error_code& err) const;
    virtual std::unique_ptr<DirIterator> dir_iterator(const fs::path& path,
                                                      std::error_code& err) const;
    virtual fs::path weakly_canonical(const fs::path& path) const;

    virtual fs::perms get_permissions(const fs::path& file) const;

    virtual fs::path remove_extension(const fs::path& path) const;
};
} // namespace multipass

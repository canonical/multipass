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

#include <multipass/file_ops.h>
#include <multipass/posix.h>

#include <fcntl.h>

namespace mp = multipass;
namespace fs = mp::fs;

mp::NamedFd::NamedFd(const fs::path& path, int fd) : path{path}, fd{fd}
{
}

mp::NamedFd::~NamedFd()
{
    if (fd != -1)
        ::close(fd);
}

mp::FileOps::FileOps(const Singleton<FileOps>::PrivatePass& pass) noexcept : Singleton<FileOps>::Singleton{pass}
{
}

bool mp::FileOps::exists(const QDir& dir) const
{
    return dir.exists();
}

bool mp::FileOps::isReadable(const QDir& dir) const
{
    return dir.isReadable();
}

QFileInfoList multipass::FileOps::entryInfoList(const QDir& dir,
                                                const QStringList& nameFilters,
                                                QDir::Filters filters,
                                                QDir::SortFlags sort) const
{
    return dir.entryInfoList(nameFilters, filters, sort);
}

bool mp::FileOps::mkpath(const QDir& dir, const QString& dirName) const
{
    return dir.mkpath(dirName);
}

bool mp::FileOps::rmdir(QDir& dir, const QString& dirName) const
{
    return dir.rmdir(dirName);
}

bool mp::FileOps::exists(const QFile& file) const
{
    return file.exists();
}

bool mp::FileOps::isDir(const QFileInfo& file) const
{
    return file.isDir();
}

bool mp::FileOps::exists(const QFileInfo& file) const
{
    return file.exists();
}

bool mp::FileOps::isReadable(const QFileInfo& file) const
{
    return file.isReadable();
}

uint mp::FileOps::ownerId(const QFileInfo& file) const
{
    return file.ownerId();
}

uint mp::FileOps::groupId(const QFileInfo& file) const
{
    return file.groupId();
}

bool mp::FileOps::is_open(const QFile& file) const
{
    return file.isOpen();
}

bool mp::FileOps::open(QFileDevice& file, QIODevice::OpenMode mode) const
{
    return file.open(mode);
}

QFileDevice::Permissions mp::FileOps::permissions(const QFile& file) const
{
    return file.permissions();
}

qint64 mp::FileOps::read(QFile& file, char* data, qint64 maxSize) const
{
    return file.read(data, maxSize);
}

QByteArray mp::FileOps::read_all(QFile& file) const
{
    return file.readAll();
}

QString mp::FileOps::read_line(QTextStream& text_stream) const
{
    return text_stream.readLine();
}

bool mp::FileOps::remove(QFile& file) const
{
    return file.remove();
}

bool mp::FileOps::rename(QFile& file, const QString& newName) const
{
    return file.rename(newName);
}

bool mp::FileOps::resize(QFile& file, qint64 sz) const
{
    return file.resize(sz);
}

bool mp::FileOps::seek(QFile& file, qint64 pos) const
{
    return file.seek(pos);
}

bool mp::FileOps::setPermissions(QFile& file, QFileDevice::Permissions permissions) const
{
    return file.setPermissions(permissions);
}

qint64 mp::FileOps::size(QFile& file) const
{
    return file.size();
}

qint64 mp::FileOps::write(QFile& file, const char* data, qint64 maxSize) const
{
    return file.write(data, maxSize);
}

qint64 mp::FileOps::write(QFileDevice& file, const QByteArray& data) const
{
    return file.write(data);
}

bool mp::FileOps::flush(QFile& file) const
{
    return file.flush();
}

bool mp::FileOps::commit(QSaveFile& file) const
{
    return file.commit();
}

std::unique_ptr<mp::NamedFd> mp::FileOps::open_fd(const fs::path& path, int flags, int perms) const
{
    const auto fd = ::open(path.string().c_str(), flags | O_BINARY, perms);
    return std::make_unique<mp::NamedFd>(path, fd);
}

int mp::FileOps::read(int fd, void* buf, size_t nbytes) const
{
    return ::read(fd, buf, nbytes);
}

int mp::FileOps::write(int fd, const void* buf, size_t nbytes) const
{
    return ::write(fd, buf, nbytes);
}

off_t mp::FileOps::lseek(int fd, off_t offset, int whence) const
{
    return ::lseek(fd, offset, whence);
}

void mp::FileOps::open(std::fstream& stream, const char* filename, std::ios_base::openmode mode) const
{
    stream.open(filename, mode);
}

bool mp::FileOps::is_open(const std::ifstream& file) const
{
    return file.is_open();
}

std::ifstream& mp::FileOps::read(std::ifstream& file, char* buffer, std::streamsize size) const
{
    file.read(buffer, size);
    return file;
}

std::unique_ptr<std::ostream> mp::FileOps::open_write(const fs::path& path, std::ios_base::openmode mode) const
{
    return std::make_unique<std::ofstream>(path, mode);
}

std::unique_ptr<std::istream> mp::FileOps::open_read(const fs::path& path, std::ios_base::openmode mode) const
{
    return std::make_unique<std::ifstream>(path, mode);
}

void mp::FileOps::copy(const fs::path& src, const fs::path& dist, fs::copy_options copy_options) const
{
    fs::copy(src, dist, copy_options);
}

bool mp::FileOps::exists(const fs::path& path, std::error_code& err) const
{
    return fs::exists(path, err);
}

bool mp::FileOps::is_directory(const fs::path& path, std::error_code& err) const
{
    return fs::is_directory(path, err);
}

bool mp::FileOps::create_directory(const fs::path& path, std::error_code& err) const
{
    return fs::create_directory(path, err);
}

bool mp::FileOps::create_directories(const fs::path& path, std::error_code& err) const
{
    return fs::create_directories(path, err);
}

bool mp::FileOps::remove(const fs::path& path, std::error_code& err) const
{
    return fs::remove(path, err);
}

void mp::FileOps::create_symlink(const fs::path& to, const fs::path& path, std::error_code& err) const
{
    fs::create_symlink(to, path, err);
}

fs::path mp::FileOps::read_symlink(const fs::path& path, std::error_code& err) const
{
    return fs::read_symlink(path, err);
}

void mp::FileOps::permissions(const fs::path& path, fs::perms perms, std::error_code& err) const
{
    fs::permissions(path, perms, err);
}

fs::file_status mp::FileOps::status(const fs::path& path, std::error_code& err) const
{
    return fs::status(path, err);
}

fs::file_status mp::FileOps::symlink_status(const fs::path& path, std::error_code& err) const
{
    return fs::symlink_status(path, err);
}

std::unique_ptr<mp::RecursiveDirIterator> mp::FileOps::recursive_dir_iterator(const fs::path& path,
                                                                              std::error_code& err) const
{
    return std::make_unique<mp::RecursiveDirIterator>(path, err);
}

std::unique_ptr<mp::DirIterator> mp::FileOps::dir_iterator(const fs::path& path, std::error_code& err) const
{
    return std::make_unique<mp::DirIterator>(path, err);
}

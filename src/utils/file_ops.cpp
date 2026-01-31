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
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/posix.h>

#include <chrono>
#include <random>
#include <stdexcept>
#include <thread>

#include <fcntl.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace fs = mp::fs;

namespace
{
constexpr static auto log_category = "FileOps";

class BackoffTimer
{
public:
    using Duration = std::chrono::milliseconds;
    BackoffTimer(Duration factor, Duration max_duration, Duration max_jitter)
        : jitter(0, max_jitter.count()), factor(factor), max_duration(max_duration)
    {
    }

    // Delay with jitter + backoff. Typical series produced by this formula would be:
    // [2, 14,23,60,90,168,216,213,218,218]
    // [14,20,30,40,98,174,221,208,206,214]
    Duration operator()(int attempt)
    {
        return std::min(max_duration, factor * (1 << (attempt - 1))) + Duration(jitter(rng));
    }

private:
    // Share the random engine with all `BackoffTimer` instances on this thread.
    thread_local static std::mt19937 rng;

    std::uniform_int_distribution<Duration::rep> jitter;
    Duration factor;
    Duration max_duration;
};

thread_local std::mt19937 BackoffTimer::rng(std::random_device{}());

} // namespace

mp::NamedFd::NamedFd(const fs::path& path, int fd) : path{path}, fd{fd}
{
}

mp::NamedFd::~NamedFd()
{
    if (fd != -1)
        ::close(fd);
}

mp::FileOps::FileOps(const Singleton<FileOps>::PrivatePass& pass) noexcept
    : Singleton<FileOps>::Singleton{pass}
{
}

void mp::FileOps::write_transactionally(const QString& file_name, const QByteArrayView& data) const
{
    using namespace std::literals::chrono_literals;
    constexpr static auto stale_lock_time = 10s;
    constexpr static auto lock_acquire_timeout = 10s;
    constexpr static auto max_attempts = 10;

    BackoffTimer backoff{/*factor=*/10ms, /*max_duration=*/200ms, /*max_jitter=*/25ms};
    const QFileInfo fi{file_name};

    if (const auto dir = fi.absoluteDir(); !mkpath(dir, "."))
        throw std::runtime_error(fmt::format("Could not create path '{}'", dir.absolutePath()));

    // Interprocess lock file to ensure that we can synchronize the request from
    // both the daemon and the client.
    QLockFile lock(fi.filePath() + ".lock");

    // Make the lock file stale after a while to avoid deadlocking on process crashes, etc.
    setStaleLockTime(lock, stale_lock_time);

    // Acquire lock file before attempting to write.
    if (!tryLock(lock, lock_acquire_timeout))
    {
        throw std::runtime_error(fmt::format("Could not acquire lock for '{}'", file_name));
    }

    // The retry logic is here because the destination file might be locked for any reason (e.g. OS
    // background indexing) so we will retry writing it until it's successful or the attempts are
    // exhausted.
    for (auto attempt = 1; attempt <= max_attempts; attempt++)
    {
        QSaveFile db_file{file_name};
        if (!open(db_file, QIODevice::WriteOnly))
            throw std::runtime_error{
                fmt::format("Could not open transactional file for writing; filename: {}",
                            file_name)};

        if (write(db_file, data.data(), data.size()) == -1)
            throw std::runtime_error{
                fmt::format("Could not write to transactional file; filename: {}; error: {}",
                            file_name,
                            db_file.errorString())};

        if (commit(db_file))
        {
            // Saved successfully
            mpl::debug(log_category,
                       "Saved file `{}` successfully in attempt #{}",
                       file_name,
                       attempt);
            return;
        }

        // Save failed. Throw if this was our last attempt.
        if (attempt == max_attempts)
            throw std::runtime_error{
                fmt::format("Could not commit transactional file; filename: {}", file_name)};

        // Otherwise, wait a bit and try again.
        const auto delay = backoff(attempt);
        mpl::warn(log_category,
                  "Failed to write `{}` in attempt #{} (reason: {}), will retry after {} ms delay.",
                  file_name,
                  attempt,
                  db_file.errorString(),
                  delay);

        std::this_thread::sleep_for(delay);
    }

    assert(false && "We should never get here");
}

// LCOV_EXCL_START

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

bool mp::FileOps::exists(const QFile& file) const
{
    return file.exists();
}

bool mp::FileOps::is_open(const QIODevice& file) const
{
    return file.isOpen();
}

bool mp::FileOps::open(QIODevice& file, QIODevice::OpenMode mode) const
{
    return file.open(mode);
}

qint64 mp::FileOps::read(QIODevice& file, char* data, qint64 maxSize) const
{
    return file.read(data, maxSize);
}

QByteArray mp::FileOps::read_all(QIODevice& file) const
{
    return file.readAll();
}

bool mp::FileOps::remove(QFile& file) const
{
    return file.remove();
}

bool mp::FileOps::rename(QFile& file, const QString& newName) const
{
    return file.rename(newName);
}

bool mp::FileOps::resize(QFileDevice& file, qint64 sz) const
{
    return file.resize(sz);
}

bool mp::FileOps::seek(QIODevice& file, qint64 pos) const
{
    return file.seek(pos);
}

qint64 mp::FileOps::size(QIODevice& file) const
{
    return file.size();
}

qint64 mp::FileOps::write(QIODevice& file, const char* data, qint64 maxSize) const
{
    return file.write(data, maxSize);
}

qint64 mp::FileOps::write(QIODevice& file, const QByteArray& data) const
{
    return file.write(data);
}

bool mp::FileOps::flush(QFileDevice& file) const
{
    return file.flush();
}

QString mp::FileOps::read_line(QTextStream& text_stream) const
{
    return text_stream.readLine();
}

bool mp::FileOps::copy(const QString& from, const QString& to) const
{
    return QFile::copy(from, to);
}

bool mp::FileOps::commit(QSaveFile& file) const
{
    return file.commit();
}

void mp::FileOps::setStaleLockTime(QLockFile& lock, std::chrono::milliseconds time) const
{
    lock.setStaleLockTime(time);
}

bool mp::FileOps::tryLock(QLockFile& lock, std::chrono::milliseconds timeout) const
{
    return lock.tryLock(timeout);
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

void mp::FileOps::open(std::fstream& stream,
                       const char* filename,
                       std::ios_base::openmode mode) const
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

std::optional<std::string> mp::FileOps::try_read_file(const fs::path& filename) const
{
    if (std::error_code err; !MP_FILEOPS.exists(filename, err) && !err)
        return std::nullopt;
    else if (err)
        throw fs::filesystem_error(
            fmt::format("error reading file {}: {}", filename, err.message()),
            err);

    const auto file = MP_FILEOPS.open_read(filename);
    file->exceptions(std::ifstream::failbit | std::ifstream::badbit);
    return std::string{std::istreambuf_iterator{*file}, {}};
}

std::unique_ptr<std::ostream> mp::FileOps::open_write(const fs::path& path,
                                                      std::ios_base::openmode mode) const
{
    return std::make_unique<std::ofstream>(path, mode);
}

std::unique_ptr<std::istream> mp::FileOps::open_read(const fs::path& path,
                                                     std::ios_base::openmode mode) const
{
    return std::make_unique<std::ifstream>(path, mode);
}

void mp::FileOps::copy(const fs::path& src,
                       const fs::path& dist,
                       fs::copy_options copy_options) const
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

void mp::FileOps::create_symlink(const fs::path& to,
                                 const fs::path& path,
                                 std::error_code& err) const
{
    fs::create_symlink(to, path, err);
}

fs::path mp::FileOps::read_symlink(const fs::path& path, std::error_code& err) const
{
    return fs::read_symlink(path, err);
}

fs::file_status mp::FileOps::status(const fs::path& path, std::error_code& err) const
{
    return fs::status(path, err);
}

fs::file_status mp::FileOps::symlink_status(const fs::path& path, std::error_code& err) const
{
    return fs::symlink_status(path, err);
}

std::unique_ptr<mp::RecursiveDirIterator>
mp::FileOps::recursive_dir_iterator(const fs::path& path, std::error_code& err) const
{
    return std::make_unique<mp::RecursiveDirIterator>(path, err);
}

std::unique_ptr<mp::DirIterator> mp::FileOps::dir_iterator(const fs::path& path,
                                                           std::error_code& err) const
{
    return std::make_unique<mp::DirIterator>(path, err);
}

fs::path mp::FileOps::weakly_canonical(const fs::path& path) const
{
    return fs::weakly_canonical(path);
}

fs::path mp::FileOps::remove_extension(const fs::path& path) const
{
    return path.parent_path() / path.stem();
}

fs::perms mp::FileOps::get_permissions(const fs::path& file) const
{
    return fs::status(file).permissions();
}

// LCOV_EXCL_STOP

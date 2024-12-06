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
 */

#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/platform_unix.h>
#include <multipass/utils.h>

#include <grp.h>
#include <scope_guard.hpp>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <libssh/sftp.h>
#include <multipass/auto_join_thread.h>

namespace mp = multipass;

namespace
{
const std::vector<std::string> supported_socket_groups{"sudo", "admin", "wheel"};

sftp_attributes_struct stat_to_attr(const struct stat* st)
{
    sftp_attributes_struct attr{};

    attr.size = st->st_size;

    attr.uid = st->st_uid;
    attr.gid = st->st_gid;

    attr.flags =
        SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME;

    attr.atime = st->st_atime;
    attr.mtime = st->st_mtime;

    attr.permissions = st->st_mode;

    return attr;
}
} // namespace

int mp::platform::Platform::chown(const char* path, unsigned int uid, unsigned int gid) const
{
    return ::lchown(path, uid, gid);
}

int mp::platform::Platform::chmod(const char* path, unsigned int mode) const
{
    return ::chmod(path, mode);
}

bool mp::platform::Platform::set_permissions(const mp::Path path, const QFileDevice::Permissions permissions) const
{
    return QFile::setPermissions(path, permissions);
}

bool mp::platform::Platform::symlink(const char* target, const char* link, bool is_dir) const
{
    return ::symlink(target, link) == 0;
}

int mp::platform::Platform::utime(const char* path, int atime, int mtime) const
{
    struct timeval tv[2];
    tv[0].tv_sec = atime;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = mtime;
    tv[1].tv_usec = 0;

    return ::lutimes(path, tv);
}

QString mp::platform::Platform::get_username() const
{
    return {};
}

std::string mp::platform::Platform::alias_path_message() const
{
    return fmt::format("You'll need to add this to your shell configuration (.bashrc, .zshrc or so) for\n"
                       "aliases to work without prefixing with `multipass`:\n\nPATH=\"$PATH:{}\"\n",
                       get_alias_scripts_folder().absolutePath());
}

void mp::platform::Platform::set_server_socket_restrictions(const std::string& server_address,
                                                            const bool restricted) const
{
    auto tokens = mp::utils::split(server_address, ":");
    if (tokens.size() != 2u)
        throw std::runtime_error(fmt::format("invalid server address specified: {}", server_address));

    const auto schema = tokens[0];
    if (schema != "unix")
        return;

    int gid{0};
    int mode{S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP};

    if (restricted)
    {
        for (const auto& socket_group : supported_socket_groups)
        {
            auto group = getgrnam(socket_group.c_str());
            if (group)
            {
                gid = group->gr_gid;
                break;
            }
        }
    }
    else
    {
        mode |= S_IROTH | S_IWOTH;
    }

    const auto socket_path = tokens[1];
    if (chown(socket_path.c_str(), 0, gid) == -1)
        throw std::runtime_error(fmt::format("Could not set ownership of the multipass socket: {}", strerror(errno)));

    if (chmod(socket_path.c_str(), mode) == -1)
        throw std::runtime_error(
            fmt::format("Could not set permissions for the multipass socket: {}", strerror(errno)));
}

QString mp::platform::Platform::multipass_storage_location() const
{
    return mp::utils::get_multipass_storage();
}

int mp::platform::symlink_attr_from(const char* path, sftp_attributes_struct* attr)
{
    struct stat st
    {
    };

    auto ret = lstat(path, &st);

    if (ret < 0)
        return ret;

    *attr = stat_to_attr(&st);

    return 0;
}

sigset_t mp::platform::make_sigset(const std::vector<int>& sigs)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    for (auto signal : sigs)
    {
        sigaddset(&sigset, signal);
    }
    return sigset;
}

sigset_t mp::platform::make_and_block_signals(const std::vector<int>& sigs)
{
    auto sigset{make_sigset(sigs)};
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    return sigset;
}

template <class Rep, class Period>
timespec make_timespec(std::chrono::duration<Rep, Period> duration)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    return timespec{seconds.count(), std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds).count()};
}

#include <iostream>

std::function<std::optional<int>(const std::function<bool()>&)> mp::platform::make_quit_watchdog(
    const std::chrono::milliseconds& timeout)
{
    return [sigset = make_and_block_signals({SIGQUIT, SIGTERM, SIGHUP, SIGUSR2}),
            timeout](const std::function<bool()>& condition) -> std::optional<int> {
        std::mutex sig_mtx;
        std::condition_variable sig_cv;
        int sig = SIGUSR2;

        // A signal generator that triggers after `timeout`
        AutoJoinThread signaler([&sig_mtx, &sig_cv, &sig, &timeout, signalee = pthread_self()] {
            std::unique_lock lock(sig_mtx);
            while (sig == SIGUSR2)
            {
                auto status = sig_cv.wait_for(lock, timeout);

                if (sig == SIGUSR2 && status == std::cv_status::timeout)
                {
                    pthread_kill(signalee, SIGUSR2);
                }
            }
        });

        // wait on signals and condition
        int ret = SIGUSR2;
        while (ret == SIGUSR2 && condition())
        {
            // can't use sigtimedwait since macOS doesn't support it
            sigwait(&sigset, &ret);
        }

        {
            std::unique_lock lock(sig_mtx);
            sig = ret == SIGUSR2 ? 0 : ret;
        }
        sig_cv.notify_all();

        // if sig is SIGUSR2 then we know we're exiting because condition() was false
        return ret == SIGUSR2 ? std::nullopt : std::make_optional(sig);
    };
}

int mp::platform::Platform::get_cpus() const
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

long long mp::platform::Platform::get_total_ram() const
{
    return static_cast<long long>(sysconf(_SC_PHYS_PAGES)) * sysconf(_SC_PAGESIZE);
}

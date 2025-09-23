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

#include <multipass/auto_join_thread.h>
#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/platform_unix.h>
#include <multipass/snap_utils.h>
#include <multipass/timer.h>
#include <multipass/utils.h>

#include <grp.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <libssh/sftp.h>

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

    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS |
                 SSH_FILEXFER_ATTR_ACMODTIME;

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

bool mp::platform::Platform::set_permissions(const std::filesystem::path& path,
                                             std::filesystem::perms permissions,
                                             bool) const
{
    // try_inherit is ignored on unix since it only pertains to ACLs

    std::error_code ec{};
    std::filesystem::permissions(path, permissions, ec);

    if (ec)
    {
        mpl::log(mpl::Level::warning,
                 "permissions",
                 fmt::format("failed to set permissions for {}: {}", path.string(), ec.message()));
    }

    return !ec;
}

bool mp::platform::Platform::take_ownership(const std::filesystem::path& path) const
{
    return this->chown(path.string().c_str(), 0, 0) == 0;
}

void mp::platform::Platform::setup_permission_inheritance(bool restricted) const
{
    if (restricted)
    {
        // only user can read/write/execute
        ::umask(~(S_IRUSR | S_IWUSR | S_IXUSR));
    }
    else
    {
        // typical default umask permissions
        ::umask(S_IWGRP | S_IWOTH);
    }
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
    return fmt::format(
        "You'll need to add this to your shell configuration (.bashrc, .zshrc or so) for\n"
        "aliases to work without prefixing with `multipass`:\n\nPATH=\"$PATH:{}\"\n",
        get_alias_scripts_folder().absolutePath());
}

void mp::platform::Platform::set_server_socket_restrictions(const std::string& server_address,
                                                            const bool restricted) const
{
    using enum std::filesystem::perms;

    auto tokens = mp::utils::split(server_address, ":");
    if (tokens.size() != 2u)
        throw std::runtime_error(
            fmt::format("invalid server address specified: {}", server_address));

    const auto schema = tokens[0];
    if (schema != "unix")
        return;

    int gid{0};
    auto mode{owner_read | owner_write | group_read | group_write};

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
        mode |= others_read | others_write;
    }

    const auto socket_path = tokens[1];
    if (chown(socket_path.c_str(), 0, gid) == -1)
        throw std::runtime_error(
            fmt::format("Could not set ownership of the multipass socket: {}", strerror(errno)));

    if (!set_permissions(socket_path, mode))
        throw std::runtime_error(fmt::format("Could not set permissions for the multipass socket"));
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

mp::platform::PosixSignal::PosixSignal(const PrivatePass& pass) noexcept : Singleton(pass)
{
}

int mp::platform::PosixSignal::pthread_sigmask(int how,
                                               const sigset_t* sigset,
                                               sigset_t* old_set) const
{
    return ::pthread_sigmask(how, sigset, old_set);
}

int mp::platform::PosixSignal::pthread_kill(pthread_t target, int signal) const
{
    return ::pthread_kill(target, signal);
}

int mp::platform::PosixSignal::sigwait(const sigset_t& sigset, int& got) const
{
    return ::sigwait(std::addressof(sigset), std::addressof(got));
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
    const auto sigset{make_sigset(sigs)};
    if (const auto ec = MP_POSIX_SIGNAL.pthread_sigmask(SIG_BLOCK, &sigset, nullptr); ec)
        throw std::runtime_error(fmt::format("Failed to block signals: {}", strerror(ec)));

    return sigset;
}

std::function<std::optional<int>(const std::function<bool()>&)> mp::platform::make_quit_watchdog(
    const std::chrono::milliseconds& period)
{
    return [sigset = make_and_block_signals({SIGQUIT, SIGTERM, SIGHUP, SIGUSR2}),
            period](const std::function<bool()>& condition) -> std::optional<int> {
        // create a timer to periodically send SIGUSR2
        utils::Timer signal_generator{period, [signalee = pthread_self()] {
                                          MP_POSIX_SIGNAL.pthread_kill(signalee, SIGUSR2);
                                      }};

        // wait on signals and condition
        int latest_signal = SIGUSR2;
        while (latest_signal == SIGUSR2 && condition())
        {
            signal_generator.start();

            // can't use sigtimedwait since macOS doesn't support it
            MP_POSIX_SIGNAL.sigwait(sigset, latest_signal);
        }

        signal_generator.stop();

        // if `latest_signal` is SIGUSR2 then we know `condition()` is false
        return latest_signal == SIGUSR2 ? std::nullopt : std::make_optional(latest_signal);
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

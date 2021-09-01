/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include <multipass/platform.h>
#include <multipass/platform_unix.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <libssh/sftp.h>

namespace mp = multipass;

namespace
{
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

std::function<int()> mp::platform::make_quit_watchdog()
{
    return [sigset = make_and_block_signals({SIGQUIT, SIGTERM, SIGHUP})]() {
        int sig = -1;
        sigwait(&sigset, &sig);
        return sig;
    };
}

/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/platform.h>
#include <multipass/platform_unix.h>

#include <unistd.h>

namespace mp = multipass;

int mp::platform::chown(const char* path, unsigned int uid, unsigned int gid)
{
    return ::chown(path, uid, gid);
}

bool mp::platform::symlink(const char* target, const char* link, bool is_dir)
{
    return ::symlink(target, link) == 0;
}

bool mp::platform::link(const char* target, const char* link)
{
    return ::link(target, link) == 0;
}

sigset_t mp::platform::make_sigset(std::vector<int> sigs)
{
    sigset_t sigset;
    sigemptyset(&sigset);
    for (auto signal : sigs)
    {
        sigaddset(&sigset, signal);
    }
    return sigset;
}

sigset_t mp::platform::make_and_block_signals(std::vector<int> sigs)
{
    auto sigset{make_sigset(sigs)};
    sigprocmask(SIG_BLOCK, &sigset, nullptr);
    return sigset;
}

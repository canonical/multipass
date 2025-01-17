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

#ifndef MULTIPASS_PLATFORM_UNIX_H
#define MULTIPASS_PLATFORM_UNIX_H

#include <vector>

#include <csignal>

#include "singleton.h"

#define MP_POSIX_SIGNAL multipass::platform::PosixSignal::instance()

namespace multipass::platform
{

class PosixSignal : public Singleton<PosixSignal>
{
public:
    PosixSignal(const PrivatePass&) noexcept;

    virtual int pthread_sigmask(int how, const sigset_t* sigset, sigset_t* old_set = nullptr) const;
    virtual int pthread_kill(pthread_t target, int signal) const;
    virtual int sigwait(const sigset_t& sigset, int& got) const;
};

sigset_t make_sigset(const std::vector<int>& sigs);
sigset_t make_and_block_signals(const std::vector<int>& sigs);

} // namespace multipass::platform
#endif // MULTIPASS_PLATFORM_UNIX_H

/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#ifndef MULTIPASS_AUTO_JOIN_THREAD_H
#define MULTIPASS_AUTO_JOIN_THREAD_H

#include <memory>
#include <thread>

namespace multipass
{

struct AutoJoinThread
{
    template <typename Callable, typename... Args>
    AutoJoinThread(Callable&& f, Args&&... args) : thread{std::forward<Callable>(f), std::forward<Args>(args)...}
    {
    }
    ~AutoJoinThread()
    {
        if (thread.joinable())
            thread.join();
    }

    std::thread thread;
};

} // namespace multipass

#endif // MULTIPASS_AUTO_JOIN_THREAD_H

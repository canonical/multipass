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

#include <condition_variable>
#include <mutex>

namespace multipass
{
struct Signal
{
    template <typename T>
    bool wait_for(const T& timeout)
    {
        std::unique_lock<std::mutex> lock{mutex};
        const auto ret = cv.wait_for(lock, timeout, [this] { return signaled; });
        signaled = false;
        return ret;
    }

    void wait()
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        cv.wait(lock, [this] { return signaled; });
        signaled = false;
    }

    void signal()
    {
        std::lock_guard<decltype(mutex)> lg{mutex};
        signaled = true;
        cv.notify_one();
    }

    std::mutex mutex;
    std::condition_variable cv;
    bool signaled{false};
};
} // namespace multipass

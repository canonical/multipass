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

#ifndef MULTIPASS_SIGNAL_H
#define MULTIPASS_SIGNAL_H

#include <condition_variable>
#include <mutex>

namespace multipass
{
namespace test
{
struct Signal
{
    template <typename T>
    bool wait_for(const T& timeout)
    {
        std::unique_lock<std::mutex> lock{mutex};
        return cv.wait_for(lock, timeout, [this] { return signaled; });
    }

    void wait()
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        cv.wait(lock, [this] { return signaled; });
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
} // namespace test
} // namespace multipass
#endif // MULTIPASS_SIGNAL_TEST_FIXTURE_H

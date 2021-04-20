/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/timer.h>

#include <multipass/top_catch_all.h>

using namespace std::chrono_literals;

namespace multipass
{
namespace utils
{

Timer::Timer(std::chrono::seconds timeout, std::function<void()> callback)
    : timeout(timeout), callback(callback), finished(false), paused(false)
{
}

Timer::~Timer()
{
    multipass::top_catch_all("timer", [this]() {
        stop();
        return 0;
    });
    if (t.joinable())
        t.join();
}

void Timer::start()
{
    t = std::thread(&Timer::main, this);
}

void Timer::main()
{
    auto remaining_time = timeout;
    std::unique_lock<std::mutex> lk(cv_m);
    while (!finished.load())
    {
        auto start_time = std::chrono::system_clock::now();
        if (!paused.load() && cv.wait_for(lk, remaining_time) == std::cv_status::timeout)
        {
            finished.store(true);
            callback();
        }
        else if (paused.load())
        {
            remaining_time = std::chrono::duration_cast<std::chrono::seconds>(
                remaining_time - (std::chrono::system_clock::now() - start_time));
            cv.wait(lk);
        }
    }
}

void Timer::pause()
{
    paused.store(true);
    cv.notify_all();
}

void Timer::resume()
{
    paused.store(false);
    cv.notify_all();
}

void Timer::stop()
{
    finished.store(true);
    cv.notify_all();
}

} // namespace utils
} // namespace multipass

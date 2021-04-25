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

namespace multipass
{
namespace utils
{

Timer::Timer(std::chrono::seconds timeout, std::function<void()> callback)
    : timeout(timeout), callback(callback), state(TimerState::Stopped)
{
}

Timer::~Timer()
{
    multipass::top_catch_all("timer", [this]() { stop(); });
}

void Timer::start()
{
    stop();

    state = TimerState::Running;
    t = std::thread(&Timer::main, this);
}

void Timer::main()
{
    auto remaining_time = timeout;
    std::unique_lock<std::mutex> lk(cv_m);
    while (state != TimerState::Stopped)
    {
        auto start_time = std::chrono::system_clock::now();
        if (state == TimerState::Running && cv.wait_for(lk, remaining_time) == std::cv_status::timeout)
        {
            state = TimerState::Stopped;
            callback();
        }
        else if (state == TimerState::Paused)
        {
            remaining_time = std::chrono::duration_cast<std::chrono::seconds>(
                remaining_time - (std::chrono::system_clock::now() - start_time));
            cv.wait(lk);
        }
    }
}

void Timer::pause()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        if (state != TimerState::Running)
            return;
        state = TimerState::Paused;
    }
    cv.notify_all();
}

void Timer::resume()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        if (state != TimerState::Paused)
            return;
        state = TimerState::Running;
    }
    cv.notify_all();
}

void Timer::stop()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        state = TimerState::Stopped;
    }
    cv.notify_all();

    if (t.joinable())
        t.join();
}

} // namespace utils
} // namespace multipass

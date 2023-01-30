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

#include <multipass/timer.h>
#include <multipass/top_catch_all.h>

namespace mpu = multipass::utils;

mpu::Timer::Timer(std::chrono::milliseconds timeout, std::function<void()> callback)
    : timeout(timeout), callback(callback), current_state(TimerState::Stopped)
{
}

mpu::Timer::~Timer()
{
    multipass::top_catch_all("timer", [this]() { stop(); });
}

void mpu::Timer::start()
{
    stop();

    current_state = TimerState::Running;
    t = std::thread(&Timer::main, this);
}

void mpu::Timer::main()
{
    auto remaining_time = timeout;
    std::unique_lock<std::mutex> lk(cv_m);
    while (current_state != TimerState::Stopped)
    {
        auto start_time = std::chrono::system_clock::now();
        if (current_state == TimerState::Running && !MP_TIMER_SYNC_FUNCS.wait_for(cv, lk, remaining_time, [this] {
                return current_state != TimerState::Running;
            }))
        {
            current_state = TimerState::Stopped;
            callback();
        }
        else if (current_state == TimerState::Paused)
        {
            remaining_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                remaining_time - (std::chrono::system_clock::now() - start_time));
            MP_TIMER_SYNC_FUNCS.wait(cv, lk, [this] { return current_state != TimerState::Paused; });
        }
    }
}

void mpu::Timer::pause()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        if (current_state != TimerState::Running)
            return;
        current_state = TimerState::Paused;
    }
    MP_TIMER_SYNC_FUNCS.notify_all(cv);
}

void mpu::Timer::resume()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        if (current_state != TimerState::Paused)
            return;
        current_state = TimerState::Running;
    }
    MP_TIMER_SYNC_FUNCS.notify_all(cv);
}

void mpu::Timer::stop()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        current_state = TimerState::Stopped;
    }
    MP_TIMER_SYNC_FUNCS.notify_all(cv);

    if (t.joinable())
        t.join();
}

mpu::TimerSyncFuncs::TimerSyncFuncs(const Singleton<TimerSyncFuncs>::PrivatePass& pass) noexcept
    : Singleton<TimerSyncFuncs>::Singleton{pass}
{
}

void mpu::TimerSyncFuncs::notify_all(std::condition_variable& cv) const
{
    cv.notify_all();
}

void mpu::TimerSyncFuncs::wait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock,
                               std::function<bool()> predicate) const
{
    cv.wait(lock, predicate);
}

bool mpu::TimerSyncFuncs::wait_for(std::condition_variable& cv, std::unique_lock<std::mutex>& lock,
                                   const std::chrono::duration<int, std::milli>& rel_time,
                                   std::function<bool()> predicate) const
{
    return cv.wait_for(lock, rel_time, predicate);
}

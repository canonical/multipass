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
        if (current_state == TimerState::Running && cv.wait_for(lk, remaining_time) == std::cv_status::timeout)
        {
            current_state = TimerState::Stopped;
            callback();
        }
        else if (current_state == TimerState::Paused)
        {
            remaining_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                remaining_time - (std::chrono::system_clock::now() - start_time));
            cv.wait(lk);
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
    cv.notify_all();
}

void mpu::Timer::resume()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        if (current_state != TimerState::Paused)
            return;
        current_state = TimerState::Running;
    }
    cv.notify_all();
}

void mpu::Timer::stop()
{
    {
        std::lock_guard<std::mutex> lk(cv_m);
        current_state = TimerState::Stopped;
    }
    cv.notify_all();

    if (t.joinable())
        t.join();
}

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

#ifndef MULTIPASS_TIMER_H
#define MULTIPASS_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <thread>

namespace multipass
{
namespace utils
{

class Timer
{
public:
    Timer(std::chrono::seconds, std::function<void()>); /* NB: callback runs on the timeout thread. */
    ~Timer();
    Timer(const Timer&) = delete;
    const Timer& operator=(const Timer&) = delete;

public:
    void start();
    void pause();
    void resume();
    void stop();

private:
    void main();
    const std::chrono::seconds timeout;
    const std::function<void()> callback;
    volatile std::atomic_bool finished;
    volatile std::atomic_bool paused;
    std::thread t;
    std::condition_variable cv;
    std::mutex cv_m;
};

} // namespace utils
} // namespace multipass

#endif // MULTIPASS_TIMER_H

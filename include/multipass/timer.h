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

#ifndef MULTIPASS_TIMER_H
#define MULTIPASS_TIMER_H

#include "disabled_copy_move.h"
#include "singleton.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace multipass::utils
{

enum class TimerState : int
{
    Stopped,
    Running,
    Paused
};

class Timer : private DisabledCopyMove
{
public:
    Timer(std::chrono::milliseconds, std::function<void()>); /* NB: callback runs on the timeout thread. */
    ~Timer();

public:
    void start();
    void pause();
    void resume();
    void stop();

private:
    void main();
    const std::chrono::milliseconds timeout;
    const std::function<void()> callback;
    TimerState current_state;
    std::thread t;
    std::condition_variable cv;
    std::mutex cv_m;
};

#define MP_TIMER_SYNC_FUNCS multipass::utils::TimerSyncFuncs::instance()

class TimerSyncFuncs : public Singleton<TimerSyncFuncs>
{
public:
    TimerSyncFuncs(const Singleton<TimerSyncFuncs>::PrivatePass&) noexcept;

    virtual void notify_all(std::condition_variable& cv) const;
    virtual void wait(std::condition_variable& cv, std::unique_lock<std::mutex>& lock, std::function<bool()>) const;
    virtual bool wait_for(std::condition_variable& cv, std::unique_lock<std::mutex>& lock,
                          const std::chrono::duration<int, std::milli>& rel_time, std::function<bool()>) const;
};
} // namespace multipass::utils

#endif // MULTIPASS_TIMER_H

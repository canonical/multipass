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

#ifndef MULTIPASS_ASYNC_PERIODIC_TASK_H
#define MULTIPASS_ASYNC_PERIODIC_TASK_H

#include <multipass/logging/log.h>

#include <chrono>
#include <future>
#include <string_view>

#include <QTimer>

namespace mpl = multipass::logging;

namespace multipass::utils
{
class AsyncPeriodicTask
{
public:
    template <typename Callable, typename... Args>
    void launch(std::string_view launch_msg, std::chrono::milliseconds msec, Callable&& func, Args&&... args)
    {
        // Log in a side thread will cause some unit tests (like launch_warns_when_overcommitting_disk) to have data
        // race on log, because the side thread log messes with the mock_logger. Because of that, we only allow the
        // main thread log for now. That is why launch_msg parameter is here. A long-term solution would be a better
        // separation of classes and mock the corresponding class function, so it will not log and mess with the
        // mock_logger in the unit tests.

        // TODO, remove the launch_msg parameter once we have better class separation.
        mpl::log(mpl::Level::info, "async task", std::string(launch_msg));
        future = std::async(std::launch::async, std::forward<Callable>(func), std::forward<Args>(args)...);
        QObject::connect(&timer, &QTimer::timeout, [launch_msg, this, func, args...]() -> void {
            // skip it if the previous one is still running
            if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                mpl::log(mpl::Level::info, "async task", std::string(launch_msg));
                future = std::async(std::launch::async, func, args...);
            }
        });
        timer.start(msec);
    }
    void start_timer()
    {
        timer.start();
    }
    void stop_timer()
    {
        timer.stop();
    }
    void wait_ongoing_task_finish() const
    {
        future.wait();
    }

private:
    QTimer timer;
    std::future<void> future;
};
} // namespace multipass::utils

#endif // MULTIPASS_TIMER_H

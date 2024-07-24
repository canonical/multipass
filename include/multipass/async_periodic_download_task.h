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

#ifndef MULTIPASS_ASYNC_PERIODIC_DOWNLOAD_TASK_H
#define MULTIPASS_ASYNC_PERIODIC_DOWNLOAD_TASK_H

#include <multipass/exceptions/download_exception.h>
#include <multipass/logging/log.h>

#include <chrono>
#include <string_view>

#include <QTimer>
#include <QtConcurrent/QtConcurrent>

namespace mpl = multipass::logging;

namespace multipass::utils
{
// Because AsyncPeriodicDownloadTask needs to be instantiated as data member of a class to function properly, and Class
// template argument deduction (CTAD) does not work for data member, so that means we can not deduced type data member
// from the below code snippet.

// template <typename Callable, typename...Args>
// class AsyncPeriodicDownloadTask
// {
// public:
//     using ReturnType = std::invoke_result_t<std::decay_t<Callable>, std::decay_t<Args>...>;
//     AsyncPeriodicDownloadTask(Callable&& func, Args&&... args);
//     ...
// };

// Therefore, the user has to specify the ReturnType as template argument while constructing it.

template <typename ReturnType = void>
class AsyncPeriodicDownloadTask
{
public:
    template <typename Callable, typename... Args>
    AsyncPeriodicDownloadTask(std::string_view launch_msg,
                              std::chrono::milliseconds normal_delay_time,
                              std::chrono::milliseconds retry_start_delay_time,
                              Callable&& func,
                              Args&&... args)
        : default_delay_time{normal_delay_time}, retry_current_delay_time{retry_start_delay_time}
    {
        // Log in a side thread will cause some unit tests (like launch_warns_when_overcommitting_disk) to have data
        // race on log, because the side thread log messes with the mock_logger. Because of that, we only allow the
        // main thread log for now. That is why launch_msg parameter is here. A long-term solution would be a better
        // separation of classes and mock the corresponding class function, so it will not log and mess with the
        // mock_logger in the unit tests.

        // TODO, remove the launch_msg parameter once we have better class separation.
        mpl::log(mpl::Level::debug, "async task", std::string(launch_msg));
        future = QtConcurrent::run(std::forward<Callable>(func), std::forward<Args>(args)...);

        auto event_handler_on_success_and_failure = [retry_start_delay_time, this]() -> void {
            try
            {
                // rethrow exception
                future.waitForFinished();
                // success case, we reset the time interval for timer and the reset the retry delay time value
                timer.start(default_delay_time);
                retry_current_delay_time = retry_start_delay_time;
            }
            catch (const multipass::DownloadException& e)
            {
                mpl::log(mpl::Level::debug,
                         "async task",
                         fmt::format("QFutureWatcher caught DownloadException {}", e.what()));
                // failure case, trigger or continue the retry mechanism
                timer.start(retry_current_delay_time);
                retry_current_delay_time = std::min(2 * retry_current_delay_time, default_delay_time);
            }
        };

        QObject::connect(&future_watcher, &QFutureWatcher<ReturnType>::finished, event_handler_on_success_and_failure);
        future_watcher.setFuture(future);

        QObject::connect(&timer, &QTimer::timeout, [launch_msg, this, func, args...]() -> void {
            // skip it if the previous one is still running
            if (future.isFinished())
            {
                mpl::log(mpl::Level::debug, "async task", std::string(launch_msg));
                future = QtConcurrent::run(func, args...);
                future_watcher.setFuture(future);
            }
        });
        timer.start(default_delay_time);
    }

    void start_timer()
    {
        timer.start();
    }
    void stop_timer()
    {
        timer.stop();
    }
    void wait_ongoing_task_finish()
    {
        future.waitForFinished();
    }

private:
    QTimer timer;
    QFuture<ReturnType> future;
    QFutureWatcher<ReturnType> future_watcher;
    const std::chrono::milliseconds default_delay_time;
    std::chrono::milliseconds retry_current_delay_time;
};
} // namespace multipass::utils

#endif

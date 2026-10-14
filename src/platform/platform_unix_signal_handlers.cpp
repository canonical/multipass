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
 */

#include <multipass/disabled_copy_move.h>
#include <multipass/logging/log.h>
#include <multipass/platform_unix.h>
#include <multipass/signal.h>

#include <QtCore>
#include <fmt/format.h>

#include <atomic>
#include <csignal>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace multipass::platform
{

namespace
{

/**
 * A mechanism for sending a notification from a signal handler
 * to a single waiting thread of the main application.
 * This class guarantees that:
 * - async_safe_notify is async-signal safe.
 * - after the first call to async_safe_notify, at least one
 * invocation of wait will return.
 * Subsequent invocations of async_safe_notify may or may not cause
 * other calls to wait to return.
 */
class AsyncSignalSafeCondition : public DisabledCopyMove
{
public:
    AsyncSignalSafeCondition()
    {
        if (pipe(sync_pipe) != 0)
        {
            const auto error = errno;
            throw std::runtime_error(
                fmt::format("Failed to create sync pipe: {}", strerror(error)));
        }

        const auto set_flags = [](int fd, int flags, int get, int set) {
            const auto prev = fcntl(fd, get);
            return prev != -1 && fcntl(fd, set, prev | flags) != -1;
        };

        if (!set_flags(sync_pipe[1], O_NONBLOCK, F_GETFL, F_SETFL) ||
            !set_flags(sync_pipe[0], FD_CLOEXEC, F_GETFD, F_SETFD) ||
            !set_flags(sync_pipe[1], FD_CLOEXEC, F_GETFD, F_SETFD))
        {
            const auto error = errno;

            close(sync_pipe[0]);
            sync_pipe[0] = -1;

            close(sync_pipe[1]);
            sync_pipe[1] = -1;

            throw std::runtime_error(
                fmt::format("Failed to configure sync pipe: {}", strerror(error)));
        }
    }

    ~AsyncSignalSafeCondition()
    {
        close(sync_pipe[0]);
        close(sync_pipe[1]);
    }

    void async_safe_notify()
    {
        const auto byte = char{};
        while (write(sync_pipe[1], &byte, 1) == -1 && errno == EINTR)
        {
        }
    }

    void wait()
    {
        while (true)
        {
            auto byte = char{};
            auto r = read(sync_pipe[0], &byte, 1);
            if (r >= 1)
            {
                return;
            }

            if (r != 0 && errno != EINTR)
            {
                const auto error = errno;
                throw std::runtime_error(
                    fmt::format("Failed to read from sync pipe: {}", strerror(error)));
            }
        }
    }

private:
    int sync_pipe[2] = {-1, -1};
};

static_assert(std::atomic_int::is_always_lock_free,
              "std::atomic_int must be lock-free to be safely used in signal handlers");

static_assert(std::atomic<AsyncSignalSafeCondition*>::is_always_lock_free,
              "std::atomic<AsyncSignalSafeCondition*> must be lock-free to be safely used "
              "in signal handlers");

static constexpr auto no_signal = -1;
static constexpr auto abort_thread = -2;

static std::atomic_int first_signal = no_signal;
static std::atomic<AsyncSignalSafeCondition*> condition = nullptr;

void signal_handler(int signo)
{
    const auto saved_errno = errno;
    if (signo == SIGTERM || signo == SIGINT || signo == SIGUSR1)
    {
        auto expected = no_signal;
        first_signal.compare_exchange_strong(expected, signo);
        condition.load()->async_safe_notify();
    }
    errno = saved_errno;
}

void register_signal_handlers()
{
    // We are intentionally leaking the condition.
    // Once used inside a signal handler, it cannot be destroyed, or we would
    // end up with undefined behavior.
    condition.store(new AsyncSignalSafeCondition());

    struct sigaction sa = {};
    sa.sa_handler = &signal_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM, &sa, nullptr) < 0 || sigaction(SIGINT, &sa, nullptr) < 0 ||
        sigaction(SIGUSR1, &sa, nullptr) < 0)
    {
        throw std::runtime_error(
            fmt::format("Failed to register signal handlers for SIGTERM, SIGINT, or SIGUSR1: {}",
                        strerror(errno)));
    }
}

} // namespace

UnixSignalHandler::UnixSignalHandler(Signal& app_ready_signal) : app_ready_signal(app_ready_signal)
{
    register_signal_handlers();
    signal_handling_thread = std::jthread{[this] { monitor_signals(); }};
}

UnixSignalHandler::~UnixSignalHandler()
{
    auto notif = condition.load();
    if (notif != nullptr)
    {
        auto expected = no_signal;
        first_signal.compare_exchange_strong(expected, abort_thread);

        notif->async_safe_notify();
    }
}

void UnixSignalHandler::monitor_signals()
{
    auto signal = first_signal.load();
    auto notif = condition.load();

    try
    {
        // If the signal handler is executed by a child process (there is a small
        // time window between fork and exec where this could happen), first_signal
        // will be modified only in the memory of the child process.
        // This checks ensure that we are waiting until the signal is received by
        // the process in which this loop is being executed.
        // Note: when a process is forked, only the thread on which the fork happen
        // will be duplicated.
        while (signal == no_signal)
        {
            notif->wait();
            signal = first_signal.load();
        }
    }
    catch (const std::exception& error)
    {
        logging::error("daemon", "Failed to wait for signal notification: {}", error.what());
    }

    if (signal == abort_thread)
    {
        return;
    }

    if (signal == SIGTERM || signal == SIGINT)
    {
        logging::info("daemon", "Received signal {} ({})", signal, strsignal(signal));
    }

    // In order to be able to gracefully end the application via QCoreApplication::quit()
    // the initialization (QT, Daemon) have to happen first. Otherwise, the application
    // might not be in a state that the QT's event loop would pick up the signal and
    // terminate. This happens when the daemon is started and being signaled in quick
    // succession.
    app_ready_signal.wait();
    QCoreApplication::quit();
}

} // namespace multipass::platform

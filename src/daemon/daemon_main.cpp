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

#include "daemon.h"
#include "daemon_config.h"
#include "daemon_init_settings.h"

#include "cli.h"

#include <multipass/constants.h>
#include <multipass/logging/log.h>
#include <multipass/platform_unix.h>
#include <multipass/signal.h>
#include <multipass/ssh/libssh_scope_guard.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/version.h>

#include <multipass/format.h>

#include <QCoreApplication>

#include <atomic>
#include <csignal>
#include <signal.h>
#include <thread>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpp = multipass::platform;

namespace
{

static_assert(std::atomic_int::is_always_lock_free,
              "std::atomic_int must be lock-free to be safely used in signal handlers");

static constexpr auto no_signal = -1;
static std::atomic_int first_signal = no_signal;
static mpp::AsyncSignalSafeNotification* signal_notif = nullptr;

void signal_handler(int signo)
{
    const auto saved_errno = errno;
    if (signo == SIGTERM || signo == SIGINT || signo == SIGUSR1)
    {
        auto expected = no_signal;
        first_signal.compare_exchange_strong(expected, signo);
        signal_notif->async_safe_notify();
    }
    errno = saved_errno;
}

void register_signal_handlers()
{
    // We are intentionally leaking the AsyncSignalSafeNotification object
    // to prevent it from being destroyed, once used inside a signal handler.
    // Not doing so could yield to undefined behavior.
    signal_notif = new mpp::AsyncSignalSafeNotification();

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

class UnixSignalHandler
{
public:
    UnixSignalHandler(mp::Signal& app_ready_signal) : app_ready_signal(app_ready_signal)
    {
        register_signal_handlers();
        signal_handling_thread = std::jthread{[this] { monitor_signals(); }};
    }

    ~UnixSignalHandler()
    {
        if (signal_notif != nullptr)
        {
            signal_notif->async_safe_notify();
        }
    }

    void monitor_signals()
    {
        try
        {
            signal_notif->wait();
        }
        catch (const std::exception& error)
        {
            mpl::error("daemon", "Failed to wait for signal notification: {}", error.what());
        }

        auto signal = first_signal.load();
        if (signal == no_signal)
        {
            return;
        }

        if (signal == SIGTERM || signal == SIGINT)
        {
            mpl::info("daemon", "Received signal {} ({})", signal, strsignal(signal));
        }

        // In order to be able to gracefully end the application via QCoreApplication::quit()
        // the initialization (QT, Daemon) have to happen first. Otherwise, the application
        // might not be in a state that the QT's event loop would pick up the signal and
        // terminate. This happens when the daemon is started and being signaled in quick
        // succession.
        app_ready_signal.wait();
        QCoreApplication::quit();
    }

private:
    mp::Signal& app_ready_signal;
    std::jthread signal_handling_thread;
};

int main_impl(int argc, char* argv[], mp::Signal& app_ready_signal)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(mp::daemon_name);
    QCoreApplication::setApplicationVersion(mp::version_string);

    mp::daemon::register_global_settings_handlers();

    auto builder = mp::cli::parse(app);
    auto config = builder.build();
    auto server_address = config->server_address;

    mp::daemon::monitor_and_quit_on_settings_change(); // TODO replace with async restart in
                                                       // relevant settings handlers

    mp::Daemon daemon(std::move(config));

    QObject::connect(
        &app,
        &QCoreApplication::aboutToQuit,
        &daemon,
        [&daemon] { mp::top_catch_all("daemon", [&daemon] { daemon.shutdown_grpc_server(); }); },
        Qt::DirectConnection);

    mpl::info("daemon", "Starting Multipass {}", mp::version_string);
    mpl::info("daemon", "Daemon arguments: {}", app.arguments().join(" "));

    // Signal the signal handler that app has completed its basic initialization, and
    // ready to process signals.
    app_ready_signal.signal();

    auto exit_code = QCoreApplication::exec();
    // QConcurrent::run() invocations are dispatched through the global
    // thread pool. Wait until all threads in the pool are properly cleaned up.
    QThreadPool::globalInstance()->waitForDone();
    mpl::info("daemon", "Goodbye!");
    return exit_code;
}
} // namespace

int main(int argc, char* argv[])
{
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    multipass::LibsshScopeGuard libssh_guard;

    mp::Signal app_ready_signal{};
    //
    // Register the signal handler as the first thing so the signal handler won't miss
    // anything.
    //
    // The signal handler will not act upon signals until either the app initializes
    // successfully, or an error happens.
    //
    UnixSignalHandler handler{app_ready_signal};
    auto exit_code = mp::top_catch_all(
        "daemon",
        [&app_ready_signal] {
            // Ensure that the signal is raised even when
            // an exception is thrown, so pending signals
            // could be processed.
            app_ready_signal.signal();
            return EXIT_FAILURE;
        },
        main_impl,
        argc,
        argv,
        app_ready_signal);
    return exit_code;
}

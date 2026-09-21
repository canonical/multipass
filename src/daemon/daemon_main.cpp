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

#include <multipass/auto_join_thread.h>
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

#include <csignal>
#include <signal.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpp = multipass::platform;

namespace
{

void signal_handler(int signo)
{
    if (signo == SIGTERM || signo == SIGINT || signo == SIGUSR1)
    {
        mpp::AsyncSignalSafeTransport::signal(signo);
    }
}

void register_signal_handlers()
{
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
    UnixSignalHandler(mp::Signal& app_ready_signal)
        : app_ready_signal(app_ready_signal), signal_handling_thread{[this] { monitor_signals(); }}
    {
        register_signal_handlers();
    }

    ~UnixSignalHandler()
    {
        pthread_kill(signal_handling_thread.thread.native_handle(), SIGUSR1);
    }

    void monitor_signals()
    {
        auto signo = signal_transport.wait();
        if (!signo.has_value())
        {
            mpl::error("daemon",
                       "Something went wrong while waiting for termination signals... "
                       "Program will be stopped.");
        }
        else if (*signo != SIGUSR1)
        {
            mpl::info("daemon", "Received signal {} ({})", *signo, strsignal(*signo));
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
    mpp::AsyncSignalSafeTransport signal_transport;
    mp::AutoJoinThread signal_handling_thread;
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

    QObject::connect(&app,
                     &QCoreApplication::aboutToQuit,
                     &daemon,
                     &mp::Daemon::shutdown_grpc_server,
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

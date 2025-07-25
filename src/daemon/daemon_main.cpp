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
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/version.h>

#include <multipass/format.h>

#include <QCoreApplication>

#include <csignal>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpp = multipass::platform;

namespace
{

class UnixSignalHandler
{
public:
    UnixSignalHandler(mp::Signal& app_ready_signal)
        : app_ready_signal(app_ready_signal),
          signal_handling_thread{
              [this, sigs = mpp::make_and_block_signals({SIGTERM, SIGINT, SIGUSR1})] {
                  monitor_signals(sigs);
              }}
    {
    }

    ~UnixSignalHandler()
    {
        pthread_kill(signal_handling_thread.thread.native_handle(), SIGUSR1);
    }

    void monitor_signals(sigset_t sigset)
    {
        int sig = -1;
        sigwait(&sigset, &sig);
        if (sig != SIGUSR1)
            mpl::log(mpl::Level::info,
                     "daemon",
                     fmt::format("Received signal {} ({})", sig, strsignal(sig)));

        // In order to be able to gracefully end the application via QCoreApplication::quit()
        // the initialization (QT, Daemon) have to happen first. Otherwise, the application
        // might not be in a state that the QT's event loop would pick up the signal and terminate.
        // This happens when the daemon is started and being signaled in quick succession.
        app_ready_signal.wait();
        QCoreApplication::quit();
    }

private:
    mp::Signal& app_ready_signal;
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
        
    mpl::log(mpl::Level::info, "daemon", fmt::format("Starting Multipass {}", mp::version_string));
    mpl::log(mpl::Level::info, "daemon", fmt::format("Daemon arguments: {}", app.arguments().join(" ")));

    // Signal the signal handler that app has completed its basic initialization, and
    // ready to process signals.
    app_ready_signal.signal();
    
    auto exit_code = QCoreApplication::exec();
    // QConcurrent::run() invocations are dispatched through the global
    // thread pool. Wait until all threads in the pool are properly cleaned up.
    QThreadPool::globalInstance()->waitForDone();
    mpl::log(mpl::Level::info, "daemon", "Goodbye!");
    return exit_code;
}
} // namespace

int main(int argc, char* argv[])
{
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

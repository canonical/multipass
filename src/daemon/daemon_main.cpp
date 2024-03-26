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
    UnixSignalHandler()
        : signal_handling_thread{
              [this, sigs = mpp::make_and_block_signals({SIGTERM, SIGINT, SIGUSR1})] { monitor_signals(sigs); }}
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
            mpl::log(mpl::Level::info, "daemon", fmt::format("Received signal {} ({})", sig, strsignal(sig)));
        QCoreApplication::quit();
    }

private:
    mp::AutoJoinThread signal_handling_thread;
};

int main_impl(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(mp::daemon_name);
    QCoreApplication::setApplicationVersion(mp::version_string);

    UnixSignalHandler handler;

    mp::daemon::register_global_settings_handlers();

    auto builder = mp::cli::parse(app);
    auto config = builder.build();
    auto server_address = config->server_address;

    mp::daemon::monitor_and_quit_on_settings_change(); // TODO replace with async restart in relevant settings handlers
    mp::Daemon daemon(std::move(config));
    QObject::connect(&app,
                     &QCoreApplication::aboutToQuit,
                     &daemon,
                     &mp::Daemon::shutdown_grpc_server,
                     Qt::DirectConnection);

    mpl::log(mpl::Level::info, "daemon", fmt::format("Starting Multipass {}", mp::version_string));
    mpl::log(mpl::Level::info, "daemon", fmt::format("Daemon arguments: {}", app.arguments().join(" ")));
    auto ret = QCoreApplication::exec();

    mpl::log(mpl::Level::info, "daemon", "Goodbye!");
    return ret;
}
} // namespace

int main(int argc, char* argv[])
{
    return mp::top_catch_all("daemon", /* fallback_return = */ EXIT_FAILURE, main_impl, argc, argv);
}

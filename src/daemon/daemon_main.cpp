/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "daemon.h"
#include "daemon_config.h"

#include <multipass/auto_join_thread.h>
#include <multipass/logging/log.h>
#include <multipass/name_generator.h>
#include <multipass/platform.h>
#include <multipass/platform_unix.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <fmt/format.h>

#include <QCoreApplication>

#include <grp.h>
#include <signal.h>
#include <sys/stat.h>
#include <vector>

namespace
{
void set_server_permissions(const std::string& server_address)
{
    QString address = QString::fromStdString(server_address);

    if (!address.startsWith("unix:"))
        return;

    auto group = getgrnam("sudo");
    if (!group)
        throw std::runtime_error("Could not determine group id for 'sudo'.");

    auto socket_path = address.section("unix:", 1, 1).toStdString();
    if (chown(socket_path.c_str(), 0, group->gr_gid) == -1)
        throw std::runtime_error("Could not set ownership of the multipass socket.");

    if (chmod(socket_path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1)
        throw std::runtime_error("Could not set permissions for the multipass socket.");
}

class UnixSignalHandler
{
public:
    UnixSignalHandler(QCoreApplication& app)
        : signal_handling_thread{[this, &app,
                                  sigs = multipass::platform::make_and_block_signals({SIGTERM, SIGINT, SIGUSR1})]{
              monitor_signals(sigs, app);
}
}
{
    }

    ~UnixSignalHandler()
    {
        pthread_kill(signal_handling_thread.thread.native_handle(), SIGUSR1);
    }

    void monitor_signals(sigset_t sigset, QCoreApplication& app)
    {
        int sig = -1;
        sigwait(&sigset, &sig);
        if (sig != SIGUSR1)
            fmt::print(stderr, "Received signal: {}\n", sig);
        app.quit();
    }

private:
    multipass::AutoJoinThread signal_handling_thread;
};
}

int main(int argc, char* argv[])
try
{
    QCoreApplication app(argc, argv);
    UnixSignalHandler handler(app);

    auto config = multipass::DaemonConfigBuilder{}.build();
    auto server_address = config->server_address;

    multipass::logging::set_logger(config->logger);
    multipass::Daemon daemon(std::move(config));

    set_server_permissions(server_address);

    app.exec();

    fmt::print(stderr, "Goodbye!\n");
    return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
    fmt::print(stderr, "Error: {}\n", e.what());
    return EXIT_FAILURE;
}

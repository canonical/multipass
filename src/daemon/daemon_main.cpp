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

#include "cli.h"

#include <multipass/auto_join_thread.h>
#include <multipass/logging/log.h>
#include <multipass/name_generator.h>
#include <multipass/platform.h>
#include <multipass/platform_unix.h>
#include <multipass/version.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <fmt/format.h>

#include <QCoreApplication>

#include <csignal>
#include <cstring>
#include <grp.h>
#include <sys/stat.h>
#include <vector>

namespace mpl = multipass::logging;
namespace mpp = multipass::platform;

namespace
{
void set_server_permissions(const std::string& server_address)
{
    QString address = QString::fromStdString(server_address);

    if (!address.startsWith("unix:"))
        return;

#ifdef MULTIPASS_PLATFORM_APPLE
    std::string group_name{"admin"};
#else
    std::string group_name{"sudo"};
#endif
    auto group = getgrnam(group_name.c_str());
    if (!group)
        throw std::runtime_error(fmt::format("Could not determine group id for '{}'", group_name));

    auto socket_path = address.section("unix:", 1, 1).toStdString();
    if (chown(socket_path.c_str(), 0, group->gr_gid) == -1)
        throw std::runtime_error("Could not set ownership of the multipass socket.");

    if (chmod(socket_path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1)
        throw std::runtime_error("Could not set permissions for the multipass socket.");
}

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
    multipass::AutoJoinThread signal_handling_thread;
};
} // namespace

int main(int argc, char* argv[]) // clang-format off
try // clang-format on
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(multipass::version_string);
    UnixSignalHandler handler;

    auto builder = multipass::cli::parse(app);
    auto config = builder.build();
    auto server_address = config->server_address;

    multipass::Daemon daemon(std::move(config));

    set_server_permissions(server_address);

    QCoreApplication::exec();

    mpl::log(mpl::Level::info, "daemon", "Goodbye!");
    return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::error, "daemon", e.what());
    return EXIT_FAILURE;
}

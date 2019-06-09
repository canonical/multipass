/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include "cli.h"

#include <multipass/auto_join_thread.h>
#include <multipass/constants.h>
#include <multipass/logging/log.h>
#include <multipass/name_generator.h>
#include <multipass/platform.h>
#include <multipass/platform_unix.h>
#include <multipass/settings.h>
#include <multipass/utils.h>
#include <multipass/version.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <multipass/format.h>

#include <QCoreApplication>
#include <QFileSystemWatcher>
#include <QObject>

#include <csignal>
#include <cstring>
#include <grp.h>
#include <sys/stat.h>
#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpp = multipass::platform;

namespace
{
const std::vector<std::string> supported_socket_groups{"sudo", "adm", "admin"};

void configure_permissions(const std::string& filename)
{
    struct group* group{nullptr};
    for (const auto socket_group : supported_socket_groups)
    {
        group = getgrnam(socket_group.c_str());
        if (group)
            break;
    }

    if (!group || chown(filename.c_str(), 0, group->gr_gid) == -1)
        throw std::runtime_error(fmt::format("Could not set file ownership on \"{}\".", filename));

    if (chmod(filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) == -1)
        throw std::runtime_error(fmt::format("Could not set file permissions on \"{}\".", filename));
}

void set_server_permissions(const std::string& server_address)
{
    auto tokens = mp::utils::split(server_address, ":");
    if (tokens.size() != 2u)
        throw std::runtime_error(fmt::format("invalid server address specified: {}", server_address));

    const auto server_name = tokens[0];
    if (server_name != "unix")
        return;

    configure_permissions(tokens[1]);
}

void init_settings(const QString& filename)
{
    QFile file{filename};
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    configure_permissions(filename.toStdString());
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
    mp::AutoJoinThread signal_handling_thread;
};

void monitor_and_quit_on_settings_change()
{
    static const auto filename = mp::Settings::get_daemon_settings_file_path();
    init_settings(filename); // create and set permissions if not there

    static QFileSystemWatcher monitor{{filename}};
    QObject::connect(&monitor, &QFileSystemWatcher::fileChanged, QCoreApplication::instance(), QCoreApplication::quit);
}

} // namespace

int main(int argc, char* argv[]) // clang-format off
try // clang-format on
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(mp::daemon_name);
    QCoreApplication::setApplicationVersion(mp::version_string);

    UnixSignalHandler handler;

    auto builder = mp::cli::parse(app);
    auto config = builder.build();
    auto server_address = config->server_address;

    monitor_and_quit_on_settings_change();
    mp::Daemon daemon(std::move(config));

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

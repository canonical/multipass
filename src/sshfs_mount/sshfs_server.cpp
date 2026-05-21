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

#include <iostream>
#include <memory>
#include <string>

#include <QStringList>

#include "sshfs_mount.h"

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/id_mappings.h>
#include <multipass/logging/log.h>
#include <multipass/logging/multiplexing_logger.h>
#include <multipass/logging/standard_logger.h>
#include <multipass/platform.h>
#include <multipass/return_codes.h>
#include <multipass/ssh/ssh_session.h>

#include <ssh/ssh_client_key_provider.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpp = multipass::platform;
using namespace std;

namespace
{
mp::id_mappings convert_id_mappings(const char* in)
{
    mp::id_mappings ret_map;
    QString input(in);

    auto maps = input.split(',', Qt::SkipEmptyParts);
    for (auto map : maps)
    {
        auto ids = map.split(":");
        if (ids.count() != 2)
        {
            cerr << "Incorrect ID mapping syntax";
            continue;
        }

        bool ok1, ok2;
        int from = ids.first().toInt(&ok1);
        int to = ids.last().toInt(&ok2);
        if (!ok1 || !ok2)
        {
            cerr << "Incorrect ID mapping ids found, ignored" << endl;
            continue;
        }

        ret_map.push_back({from, to});
    }

    return ret_map;
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 9)
    {
        cerr << "Incorrect arguments" << endl;
        return 2;
    }

    const auto key = qgetenv("KEY");
    if (key == nullptr)
    {
        cerr << "KEY not set" << endl;
        return 2;
    }
    const auto priv_key_blob = string(key);
    const auto host = string(argv[1]);
    const int port = atoi(argv[2]);
    const auto username = string(argv[3]);
    const auto source_path = string(argv[4]);
    const auto target_path = string(argv[5]);
    const mp::id_mappings uid_mappings = convert_id_mappings(argv[6]);
    const mp::id_mappings gid_mappings = convert_id_mappings(argv[7]);
    const mpl::Level log_level = static_cast<mpl::Level>(atoi(argv[8]));

    auto base_logger = mpp::make_logger(log_level);
    if (!base_logger)
        base_logger = std::make_unique<mpl::StandardLogger>(log_level);

    // Use the MultiplexingLogger as we may end up routing messages to the daemon too at some point
    auto mpx_logger = std::make_shared<mpl::MultiplexingLogger>(std::move(base_logger));
    mpl::set_logger(mpx_logger);

    MP_PLATFORM.setup_permission_inheritance(false);

    try
    {
        auto watchdog = mpp::make_quit_watchdog(
            std::chrono::milliseconds{500}); // called while there is only one thread

        mp::SSHSession session{host, port, username, mp::SSHClientKeyProvider{priv_key_blob}};
        mp::SshfsMount sshfs_mount(std::move(session),
                                   source_path,
                                   target_path,
                                   gid_mappings,
                                   uid_mappings);

        // ssh lives on its own thread, use this thread to listen for quit signal
        auto sig = watchdog([&sshfs_mount] { return sshfs_mount.alive(); });

        if (sig.has_value())
            cout << "Received signal " << *sig << ". Stopping" << endl;
        else
            cerr << "SFTP server thread stopped unexpectedly." << endl;

        sshfs_mount.stop();
        // If the sshfs thread is detached, `return` would cause a use-after-free
        // so we use exit to abort the underlying thread if it is still running.
        // Any other resources are given back to the OS.
        exit(sig.has_value() ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    catch (const mp::SSHFSMissingError&)
    {
        cerr << "SSHFS was not found on the host: " << host << endl;
        // ssh resource teardown already happened, so exit() is not necessary
        return mp::missing_host_sshfs_exit_code;
    }
    catch (const exception& e)
    {
        cerr << e.what();
        // ssh resource teardown already happened, so exit() is not necessary
        return EXIT_FAILURE;
    }
}

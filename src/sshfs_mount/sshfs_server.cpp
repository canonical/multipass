/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include "../ssh/ssh_client_key_provider.h" // FIXME
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>
#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/sshfs_mount/sshfs_mount.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpp = multipass::platform;
using namespace std;

namespace
{
unordered_map<int, int> deserialise_id_map(const char* in)
{
    unordered_map<int, int> id_map;
    QString input(in);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    auto maps = input.split(',', Qt::SkipEmptyParts);
#else
    auto maps = input.split(',', QString::SkipEmptyParts);
#endif
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
        id_map.insert({from, to});
    }
    return id_map;
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc != 8)
    {
        cerr << "Incorrect arguments" << endl;
        exit(2);
    }

    const auto key = qgetenv("KEY");
    if (key == nullptr)
    {
        cerr << "KEY not set" << endl;
        exit(2);
    }
    const auto priv_key_blob = string(key);
    const auto host = string(argv[1]);
    const int port = atoi(argv[2]);
    const auto username = string(argv[3]);
    const auto source_path = string(argv[4]);
    const auto target_path = string(argv[5]);
    const unordered_map<int, int> uid_map = deserialise_id_map(argv[6]);
    const unordered_map<int, int> gid_map = deserialise_id_map(argv[7]);

    auto logger = std::make_shared<mpl::StandardLogger>(mpl::Level::error); // QUESTION - how to pass verbosity level?
    mpl::set_logger(logger);

    try
    {
        auto watchdog = mpp::make_quit_watchdog(); // called while there is only one thread

        mp::SSHSession session{host, port, username, mp::SSHClientKeyProvider{priv_key_blob}};
        mp::SshfsMount sshfs_mount(move(session), source_path, target_path, gid_map, uid_map);

        // ssh lives on its own thread, use this thread to listen for quit signal
        if (int sig = watchdog())
            cout << "Received signal " << sig << ". Stopping" << endl;

        sshfs_mount.stop();
        exit(0);
    }
    catch (const mp::SSHFSMissingError&)
    {
        cerr << "SSHFS was not found on the host: " << host << endl;
        exit(9);
    }
    catch (const exception& e)
    {
        cerr << e.what();
    }
    return 1;
}

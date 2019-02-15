/*
 * Copyright (C) 2018 Canonical, Ltd.
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
#include <string>

#include "../ssh/ssh_client_key_provider.h" // FIXME
#include <multipass/ssh/ssh_session.h>
#include <multipass/sshfs_mount/sshfs_mount.h>

#include <signal.h>
#include <sys/prctl.h>

namespace mp = multipass;

namespace
{
std::unordered_map<int, int> deserialise_id_map(const char* in)
{
    std::unordered_map<int, int> id_map;
    QString input(in);
    auto maps = input.split(',', QString::SkipEmptyParts);
    for (auto map : maps)
    {
        auto ids = map.split(":");
        if (ids.count() != 2)
        {
            std::cerr << "Incorrect ID mapping syntax";
            continue;
        }
        bool ok1, ok2;
        int from = ids.first().toInt(&ok1);
        int to = ids.last().toInt(&ok2);
        if (!ok1 || !ok2)
        {
            std::cerr << "Incorrect ID mapping ids found, ignored" << std::endl;
            continue;
        }
        id_map.insert({from, to});
    }
    return id_map;
}
} // namespace

int main(int argc, char* argv[])
{
    prctl(PR_SET_PDEATHSIG, SIGHUP); // ensure if parent dies, this process gets the SIGHUP signal

    if (argc != 8)
    {
        std::cerr << "Incorrect arguments" << std::endl;
        return -1;
    }

    const auto key = getenv("KEY");
    if (key == nullptr)
    {
        std::cerr << "KEY not set" << std::endl;
        return -2;
    }
    const auto priv_key_blob = std::string(key);
    const auto host = std::string(argv[1]);
    const int port = atoi(argv[2]);
    const auto username = std::string(argv[3]);
    const auto source_path = std::string(argv[4]);
    const auto target_path = std::string(argv[5]);
    const std::unordered_map<int, int> gid_map = deserialise_id_map(argv[6]);
    const std::unordered_map<int, int> uid_map = deserialise_id_map(argv[7]);

    try
    {
        mp::SSHSession session{host, port, username, mp::SSHClientKeyProvider{priv_key_blob}};

        auto sshfs_mount =
            std::make_unique<mp::SshfsMount>(std::move(session), source_path, target_path, gid_map, uid_map);

        // ssh lives on its own thread, use this thread to listen for quit signal
        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGQUIT);
        sigaddset(&sigset, SIGTERM);
        sigaddset(&sigset, SIGHUP);
        sigprocmask(SIG_BLOCK, &sigset, nullptr);
        int sig = -1;

        sigwait(&sigset, &sig);
        std::cout << "Received signal " << sig << ". Stopping" << std::endl;
        sshfs_mount->stop();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}

/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#ifndef MULTIPASS_SSH_CLIENT_H
#define MULTIPASS_SSH_CLIENT_H

#include <multipass/console.h>
#include <multipass/ssh/ssh_session.h>

#include <libssh/libssh.h>

#include <memory>
#include <string>
#include <vector>

namespace multipass
{
using SSHSessionUPtr = std::unique_ptr<SSHSession>;

class SSHClient
{
public:
    using ChannelUPtr = std::unique_ptr<ssh_channel_struct, void (*)(ssh_channel)>;

    SSHClient(const std::string& host, int port, const std::string& username, const std::string& priv_key_blob);
    SSHClient(SSHSessionUPtr ssh_session, Console::UPtr console = nullptr);

    int exec(const std::vector<std::string>& args);
    void connect();

private:
    void handle_ssh_events();

    SSHSessionUPtr ssh_session;
    ChannelUPtr channel;
    Console::UPtr console;
};
}
#endif // MULTIPASS_SSH_CLIENT_H

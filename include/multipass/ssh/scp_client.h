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

#ifndef MULTIPASS_SCP_CLIENT_H
#define MULTIPASS_SCP_CLIENT_H

#include <multipass/ssh/ssh_session.h>

#include <libssh/libssh.h>

#include <memory>
#include <string>
#include <vector>

namespace multipass
{
using SSHSessionUPtr = std::unique_ptr<SSHSession>;

class SCPClient
{
public:
    SCPClient(const std::string& host, int port, const std::string& username, const std::string& priv_key_blob);
    SCPClient(SSHSessionUPtr ssh_session);

    void push_file(const std::string& source_path, const std::string& destination_path);
    void pull_file(const std::string& source_path, const std::string& destination_path);

private:
    SSHSessionUPtr ssh_session;
};
}
#endif // MULTIPASS_SCP_CLIENT_H

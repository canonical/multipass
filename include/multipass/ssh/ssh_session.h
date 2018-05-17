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

#ifndef MULTIPASS_SSH_H
#define MULTIPASS_SSH_H

#include <multipass/ssh/ssh_process.h>

#include <libssh/libssh.h>

#include <memory>
#include <string>

namespace multipass
{
class SSHKeyProvider;
class SSHSession
{
public:
    SSHSession(const std::string& host, int port);
    SSHSession(const std::string& host, int port, const std::string& ssh_username, const SSHKeyProvider& key_provider);

    SSHProcess exec(const std::string& cmd);

    void force_shutdown();

private:
    operator ssh_session() const;

    SSHSession(const std::string& host, int port, const std::string& ssh_username, const SSHKeyProvider* key_provider);
    std::unique_ptr<ssh_session_struct, void(*)(ssh_session)> session;

    friend class SCPClient;
    friend class SSHClient;
    friend class SftpServer;
};
}
#endif // MULTIPASS_SSH_H

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

#pragma once

#include <multipass/singleton.h>
#include <multipass/ssh/sftp_client.h>
#include <multipass/ssh/ssh_client.h>
#include <multipass/ssh/ssh_coordinates.h>

#include <memory>

#define MP_CLIENT_SSH_FACTORY multipass::ClientSSHFactory::instance()

namespace multipass
{
class Terminal;

using SSHClientUPtr = std::unique_ptr<SSHClient>;
using SftpClientUPtr = std::unique_ptr<SFTPClient>;

class ClientSSHFactory : public Singleton<ClientSSHFactory>
{
public:
    ClientSSHFactory(const Singleton<ClientSSHFactory>::PrivatePass&) noexcept;

    virtual SSHClientUPtr make_ssh_client(const SSHCoordinates& ssh_coordinates,
                                          Terminal* term) const;
    virtual SftpClientUPtr make_sftp_client(const SSHCoordinates& ssh_coordinates) const;
};
} // namespace multipass

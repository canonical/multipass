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

#include <multipass/ssh/client_ssh_factory.h>
#include <multipass/ssh/plain_sftp_client.h>
#include <multipass/ssh/platform_ssh_client.h>

namespace mp = multipass;

mp::ClientSSHFactory::ClientSSHFactory(
    const Singleton<ClientSSHFactory>::PrivatePass& pass) noexcept
    : Singleton<ClientSSHFactory>::Singleton{pass}
{
}

mp::SSHClientUPtr mp::ClientSSHFactory::make_ssh_client(const mp::SSHCoordinates& ssh_coordinates,
                                                        Terminal* term) const
{
    return std::make_unique<mp::PlatformSSHClient>(mp::PlainSSHSession{ssh_coordinates}, term);
}

mp::SftpClientUPtr mp::ClientSSHFactory::make_sftp_client(
    const mp::SSHCoordinates& ssh_coordinates) const
{
    return std::make_unique<mp::PlainSFTPClient>(mp::PlainSSHSession{ssh_coordinates});
}

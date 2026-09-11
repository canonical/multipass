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

#include <multipass/sshfs_mount/sftp_client_steward.h>

namespace multipass
{

/**
 * An SftpClientSteward that discovers what sshfs is available and how it should be run
 */
class SshfsClientSteward final : public SftpClientSteward
{
public:
    std::string compose_client_command(SSHSession& session,
                                       const std::string& source,
                                       const std::string& target) const override;

    /**
     * @copydoc SftpClientSteward::clean_up_after_client
     *
     * This implementation unmounts stale mounts that may have been left over by the sshfs client.
     */
    void clean_up_after_client(SSHSession& session, const std::string& source) const override;
};
} // namespace multipass

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

#include "multipass/disabled_copy_move.h"

#include <string>

namespace multipass
{
class SSHSession;

/**
 * A steward for the remote SFTP. It composes the command that runs the client to provide an
 * SFTP-based mount, once paired with an SFTP server, and it clears whatever such a client leaves
 * behind when it dies.
 */
class SftpClientSteward : private DisabledCopyMove
{
public:
    virtual ~SftpClientSteward() = default;

    // No copies
    SftpClientSteward(const SftpClientSteward&) = delete;
    SftpClientSteward& operator=(const SftpClientSteward&) = delete;

    /**
     * Compose the command to mount the @p source directory onto the @p target directory on the
     * remote end.
     *
     * @param session A session into the remote environment, which can be used to probe it.
     * @param source The source path that the client should mount.
     * @param target The target path for the client to mount it on.
     * @return A client command that can be run on the remote end to mount @p source onto @p target
     * over SFTP.
     */
    virtual std::string compose_client_command(SSHSession& session,
                                               const std::string& source,
                                               const std::string& target) const = 0;

    /**
     * Clear up whatever the sftp client may have left behind. Call this before attempting to
     * replace a dead client.
     *
     * @param session A session into the remote environment to clean up.
     * @param source The source path that the client was asked to mount.
     */
    virtual void clean_up_after_client(SSHSession& session, const std::string& source) const = 0;

protected:
    SftpClientSteward() = default;
};
} // namespace multipass

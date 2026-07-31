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
 * Composer of an SFTP client command that can be run remotely to provide an SFTP-based mount, once
 * paired with an SFTP server.
 */
class SftpClientComposer : private DisabledCopyMove
{
public:
    virtual ~SftpClientComposer() = default;

    // No copies
    SftpClientComposer(const SftpClientComposer&) = delete;
    SftpClientComposer& operator=(const SftpClientComposer&) = delete;

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

protected:
    SftpClientComposer() = default;
};
} // namespace multipass

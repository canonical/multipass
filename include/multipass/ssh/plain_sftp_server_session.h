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

#include <multipass/private_pass_provider.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/sshfs_mount/sftp_server_session.h>

#include <libssh/sftp.h>

namespace multipass
{
class PlainSftpServerSession : public SftpServerSession,
                               public PrivatePassProvider<PlainSftpServerSession>
{
public:
    explicit PlainSftpServerSession(PlainSSHSession&& ssh_session);
    PlainSftpServerSession(const PlainSftpServerSession&) = delete;
    PlainSftpServerSession& operator=(const PlainSftpServerSession&) = delete;

    // TODO@sftp Make class final before enabling these
    PlainSftpServerSession(PlainSftpServerSession&&) = delete;
    PlainSftpServerSession& operator=(PlainSftpServerSession&&) = delete;

private:
    using SftpSessionUptr = std::unique_ptr<sftp_session_struct, decltype(sftp_server_free)*>;

    PlainSSHSession ssh_session;
    SftpSessionUptr sftp_session;
};
} // namespace multipass

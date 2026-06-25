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
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/sshfs_mount/sftp_session.h>

#include <libssh/sftp.h>

namespace multipass
{

/**
 * A concrete SftpSession backed by an SSHFS mount: it serves the SFTP protocol over an SSH
 * session to a remote sshfs client, which mounts it on the guest.
 */
class PlainSftpSession : public SftpSession, public PrivatePassProvider<PlainSftpSession>
{
public:
    PlainSftpSession(PlainSSHSession&& ssh_session_obj, const std::string& sshfs_cmd);
    PlainSftpSession(const PlainSftpSession&) = delete;
    PlainSftpSession& operator=(const PlainSftpSession&) = delete;

    // TODO@sftp Make class final before enabling these
    PlainSftpSession(PlainSftpSession&&) = delete;
    PlainSftpSession& operator=(PlainSftpSession&&) = delete;

private:
    using RawSftpSessionUptr = std::unique_ptr<sftp_session_struct, decltype(sftp_server_free)*>;

    static RawSftpSessionUptr make_raw_sftp_session(ssh_session raw_session, ssh_channel channel);

    PlainSSHSession plain_ssh_session;
    std::unique_ptr<PlainSSHProcess> sshfs_process;
    RawSftpSessionUptr raw_sftp_session;
};
} // namespace multipass

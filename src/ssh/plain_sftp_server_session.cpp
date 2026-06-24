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

#include <multipass/ssh/plain_sftp_server_session.h>

#include <multipass/exceptions/ssh_exception.h>

#include <fmt/format.h>

#include <utility>

extern "C"
{
int sftp_reply_version(sftp_client_message msg);
}

namespace mp = multipass;

mp::PlainSftpServerSession::SftpSessionUptr
mp::PlainSftpServerSession::make_sftp_session(::ssh_session session, ssh_channel channel)
{
    SftpSessionUptr sftp_server_session{sftp_server_new(session, channel), sftp_server_free};
    // The function sftp_server_init was expanded here to avoid deprecation warnings.
    // TODO: move to callback-based sftp implementations.
    // https://github.com/canonical/multipass/issues/4445

    /* handles setting the sftp->client_version */
    sftp_client_message msg{sftp_get_client_message(sftp_server_session.get())};
    if (msg == nullptr)
    {
        throw mp::SSHException("[sftp] server init failed: 'Null client message'");
    }

    if (msg->type != SSH_FXP_INIT)
    {
        throw mp::SSHException(fmt::format(
            "[sftp] server init failed: 'FATAL: Packet read of type {} instead of SSH_FXP_INIT'",
            msg->type));
    }

    // Optional: Log the SSH_FXP_INIT reception like libssh does with SSH_LOG but with mp::log

    if (sftp_reply_version(msg) != SSH_OK)
    {
        throw mp::SSHException(
            "[sftp] server init failed: 'FATAL: Failed to process the SSH_FXP_INIT message'");
    }

    return sftp_server_session;
}

mp::PlainSftpServerSession::PlainSftpServerSession(PlainSSHSession&& session)
    : ssh_session{std::move(session)},
      sftp_session{make_sftp_session(ssh_session.borrow_session(pass), nullptr)}
// TODO@sftp sshfs process and channel
{
}

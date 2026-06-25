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
#include <multipass/ssh/plain_ssh_process.h>

#include <fmt/format.h>

#include <chrono>
#include <stdexcept>
#include <utility>

extern "C"
{
int sftp_reply_version(sftp_client_message msg);
}

namespace mp = multipass;

namespace
{
using namespace std::literals::chrono_literals;

void check_sshfs_status(mp::SSHProcess& sshfs_process)
{
    if (sshfs_process.exit_recognized(250ms))
    {
        // This `if` is artificial and should not really be here. However there is a complex
        // arrangement of Sftp and SshfsMount tests depending on this.
        // TODO@sftp no longer needed - just write new tests properly
        if (sshfs_process.exit_code(250ms) != 0) // TODO remove
            throw std::runtime_error(sshfs_process.read_std_error());
    }
}

auto create_sshfs_process(mp::SSHSession& session, const std::string& sshfs_cmd)
{
    auto sshfs_process = session.exec(sshfs_cmd);

    check_sshfs_status(*sshfs_process);

    return sshfs_process;
}
} // namespace

mp::PlainSftpServerSession::SftpSessionUptr
mp::PlainSftpServerSession::make_sftp_session(ssh_session session, ssh_channel channel)
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

mp::PlainSftpServerSession::PlainSftpServerSession(PlainSSHSession&& session,
                                                   const std::string& sshfs_cmd)
    : plain_ssh_session{std::move(session)},
      sshfs_process{create_sshfs_process(plain_ssh_session, sshfs_cmd)},
      sftp_session{make_sftp_session(
          plain_ssh_session.borrow_session(pass),
          static_cast<PlainSSHProcess*>(sshfs_process.get())->borrow_channel(pass))}
// TODO@rewiressh no cast
{
}

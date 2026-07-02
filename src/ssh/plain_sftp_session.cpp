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

#include <multipass/ssh/plain_sftp_session.h>

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/ssh/plain_sftp_client_message.h>
#include <multipass/ssh/plain_ssh_process.h>

#include <fmt/format.h>

#include <libssh/sftp.h>

#include <cassert>
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

auto create_sshfs_process(mp::PlainSSHSession& session, const std::string& sshfs_cmd)
{
    auto sshfs_process = session.exec_plain(sshfs_cmd);

    check_sshfs_status(*sshfs_process);

    return sshfs_process;
}
} // namespace

void mp::PlainSftpSession::RawSftpSessionDeleter::operator()(sftp_session msg) const noexcept
{
    sftp_server_free(msg);
}

mp::PlainSftpSession::RawSftpSessionUptr
mp::PlainSftpSession::make_raw_sftp_session(ssh_session raw_session, ssh_channel channel)
{
    // The function sftp_server_init was expanded here to avoid deprecation warnings.
    // TODO: move to callback-based sftp implementations.
    // https://github.com/canonical/multipass/issues/4445

    // TODO@sftp go through MP_LIBSSH
    RawSftpSessionUptr raw_sftp_session{sftp_server_new(raw_session, channel)};
    if (!raw_sftp_session)
        throw SSHException(
            fmt::format("[sftp] server init failed: could not create a new sftp_server."));

    constexpr static int init_timeout_ms = 5000;
    constexpr static auto init_error_prefix = "[sftp] server init failed:";
    int poll_result = ssh_channel_poll_timeout(raw_sftp_session->channel,
                                               init_timeout_ms,
                                               /* is_stderr = */ 0);
    if (poll_result <= 0)
    {
        std::string init_error_detail{""};
        if (poll_result < 0)
        {
            assert((poll_result == SSH_ERROR || poll_result == SSH_EOF) &&
                   "contract includes no other negative numbers");
            init_error_detail = "connection drop or malfunction";
        }
        else
            init_error_detail = "timed out waiting for the initial client message";

        throw SSHException{fmt::format("{}: {}", init_error_prefix, init_error_detail)};
    }

    /* handles setting the sftp->client_version */
    // TODO@sftp no leak plz - use SftpMessage
    sftp_client_message msg{sftp_get_client_message(raw_sftp_session.get())};
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

    return raw_sftp_session;
}

mp::PlainSftpSession::PlainSftpSession(PlainSSHSession&& ssh_session_obj,
                                       const std::string& sshfs_cmd)
    : plain_ssh_session{std::move(ssh_session_obj)},
      sshfs_process{create_sshfs_process(plain_ssh_session, sshfs_cmd)},
      raw_sftp_session{make_raw_sftp_session(plain_ssh_session.borrow_session(pass),
                                             sshfs_process->borrow_channel(pass))}
{
}

void mp::PlainSftpSession::request_stop() noexcept
{
    stop_requested.store(true);
}

std::unique_ptr<mp::SftpMessage> mp::PlainSftpSession::next_message()
{
    while (!stop_requested.load())
    {
        int not_stderr = 0;
        int poll_result =
            ssh_channel_poll_timeout(raw_sftp_session->channel, poll_interval.count(), not_stderr);

        if (poll_result < 0)
        {
            assert((poll_result == SSH_ERROR || poll_result == SSH_EOF) &&
                   "contract includes no other negative numbers");
            return nullptr; // connection drop or malfunction
        }

        if (poll_result == 0)
            continue; // nothing to read yet; recheck the stop flag

        sftp_client_message raw_msg{sftp_get_client_message(raw_sftp_session.get())};
        if (raw_msg == nullptr)
            return nullptr; // connection dropped or desynced

        return std::make_unique<PlainSftpMessage>(raw_msg);
    }

    return nullptr; // stop requested
}

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

#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/libssh_wrapper.h>
#include <multipass/ssh/plain_sftp_message.h>
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/sshfs_mount/sftp_client_composer.h>

#include <fmt/format.h>

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
using namespace std::literals::chrono_literals;

constexpr auto category = "sftp session";

class SftpInitException : public mp::SSHException
{
public:
    SftpInitException(const std::string& detail)
        : SSHException{fmt::format("{}: {}", init_error_prefix, detail)}
    {
    }

private:
    constexpr static auto init_error_prefix = "[sftp] server init failed:"; // TODO square brackets?
};

void check_sshfs_status(mp::SSHProcess& sshfs_process)
{
    // TODO@sftp should we have a way to wait for it to start running...
    if (sshfs_process.exit_recognized(250ms)) // TODO@sftp don't we need to try-catch this?
        throw std::runtime_error(sshfs_process.read_std_error());
}

auto create_sshfs_process(mp::PlainSSHSession& session, const std::string& sshfs_cmd)
{
    auto sshfs_process = session.exec_plain(sshfs_cmd);

    assert(sshfs_process && "can't have null process");
    check_sshfs_status(*sshfs_process);

    return sshfs_process;
}

int poll_stdout(ssh_channel channel, int timeout)
{
    int poll_result = MP_LIBSSH.ssh_channel_poll_timeout(channel, timeout, /* is_stderr = */ 0);
    if (poll_result < 0)
        assert((poll_result == SSH_ERROR || poll_result == SSH_EOF) &&
               "contract includes no other negative numbers");

    return poll_result;
}
} // namespace

void mp::PlainSftpSession::RawSftpSessionDeleter::operator()(sftp_session session) const noexcept
{
    MP_LIBSSH.sftp_server_free(session);
}

mp::PlainSftpSession::RawSftpSessionUptr
mp::PlainSftpSession::make_raw_sftp_session(ssh_session raw_session, ssh_channel channel)
{
    // libssh internals used in sftp_server_init are expanded here to avoid deprecation warnings.
    // TODO: move to callback-based sftp implementations.
    // https://github.com/canonical/multipass/issues/4445

    constexpr static long give_up_timeout_secs = 5; // libssh reads SSH_OPTIONS_TIMEOUT as long

    RawSftpSessionUptr raw_sftp_session{MP_LIBSSH.sftp_server_new(raw_session, channel)};
    if (!raw_sftp_session)
        throw SSHException(
            fmt::format("[sftp] server init failed: could not create a new sftp_server."));

    // Bound reads/writes to avoid indefinite blocks in mid-message reads, within next_message().
    if (MP_LIBSSH.ssh_options_set(raw_session, SSH_OPTIONS_TIMEOUT, &give_up_timeout_secs) !=
        SSH_OK)
    {
        const auto raw_error = MP_LIBSSH.ssh_get_error(raw_session);
        throw SftpInitException{fmt::format("could not set session timeout: '{}'", raw_error)};
    }

    int res = poll_stdout(raw_sftp_session->channel, static_cast<int>(give_up_timeout_secs * 1000));
    if (res <= 0)
    {
        const auto err_detail = res < 0 ? "connection drop or malfunction"
                                        : "timed out waiting for the initial client message";
        throw SftpInitException{err_detail};
    }

    /* handles setting the sftp->client_version */
    // TODO@sftp no leak plz - use SftpMessage
    sftp_client_message msg{MP_LIBSSH.sftp_get_client_message(raw_sftp_session.get())};
    if (msg == nullptr)
        throw SftpInitException{"Null client message"};

    if (msg->type != SSH_FXP_INIT)
        throw SftpInitException{
            fmt::format("FATAL: Packet read of type {} instead of SSH_FXP_INIT", msg->type)};

    // Optional: Log the SSH_FXP_INIT reception like libssh does with SSH_LOG but with mp::log

    if (MP_LIBSSH.sftp_reply_version(msg) != SSH_OK)
    {
        throw SftpInitException{"FATAL: Failed to process the SSH_FXP_INIT message"};
    }

    return raw_sftp_session;
}

mp::PlainSftpSession::PlainSftpSession(PlainSSHSession&& ssh_session_obj,
                                       const SftpClientComposer& client_composer,
                                       const std::string& source,
                                       const std::string& target)
    : plain_ssh_session{std::move(ssh_session_obj)},
      sshfs_cmd{client_composer.compose_client_command(plain_ssh_session, source, target)}
{
    spawn_client();
}

void mp::PlainSftpSession::spawn_client()
{
    assert(!sshfs_process && "precondition - no client may be running");

    sshfs_process = create_sshfs_process(plain_ssh_session, sshfs_cmd);
    raw_sftp_session = make_raw_sftp_session(plain_ssh_session.borrow_session(pass),
                                             sshfs_process->borrow_channel(pass));
}

void mp::PlainSftpSession::renew_client()
{
    if (!stop_requested.load())
    {
        mpl::debug(category, "Attempting SFTP client recovery.");

        raw_sftp_session.reset(); // mind the order: this borrows the process's channel
        sshfs_process.reset();

        spawn_client();
    }
    else
        mpl::debug(category, "Skipping SFTP client recovery due to stop request.");
}

void mp::PlainSftpSession::request_stop() noexcept
{
    stop_requested.store(true);
}

std::unique_ptr<mp::SftpMessage> mp::PlainSftpSession::next_message()
{
    sftp_client_message raw_msg = nullptr;
    while (!stop_requested.load())
    {
        int poll_result = poll_stdout(raw_sftp_session->channel, poll_interval.count());
        if (poll_result > 0) // bounded by session timeout
            raw_msg = MP_LIBSSH.sftp_get_client_message(raw_sftp_session.get());
        else if (poll_result == 0)
            continue; // nothing to read yet

        break; // at this point, we either have a message or a connection drop/desync/malfunction
    }

    return raw_msg ? std::make_unique<PlainSftpMessage>(*raw_msg) : nullptr;
}

bool mp::PlainSftpSession::client_failed()
{
    try
    {
        // TODO@sftp distinguish 0 from no return, once that SSHProcess API has settled
        return sshfs_process->exit_code(250ms) != 0;
    }
    catch (const ExitlessSSHProcessException&)
    {
        return true;
    }
}

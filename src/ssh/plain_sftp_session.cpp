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
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/libssh_wrapper.h>
#include <multipass/ssh/plain_sftp_message.h>
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/sshfs_mount/sftp_client_steward.h>
#include <multipass/top_catch_all.h>

#include <scope_guard.hpp>

#include <cassert>
#include <chrono>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
using namespace std::literals::chrono_literals;

constexpr auto category = "sftp session";
constexpr auto client_exit_timeout = 250ms; // how long we wait on the client's exit status

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

void check_client_status(mp::SSHProcess& client_process)
{
    // should we have a way to wait for it to start running?
    if (client_process.exit_recognized(client_exit_timeout))
        throw mp::SSHException(client_process.read_std_error());
}

auto create_client_process(mp::PlainSSHSession& session, const std::string& client_cmd)
{
    auto client_process = session.exec_plain(client_cmd);

    assert(client_process && "can't have null process");
    check_client_status(*client_process);

    return client_process;
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
    sftp_client_message msg{MP_LIBSSH.sftp_get_client_message(raw_sftp_session.get())};
    if (msg == nullptr)
        throw SftpInitException{"Null client message"};

    PlainSftpMessage wrapped{*msg};
    if (wrapped.type() != mp::SftpMessageType::init)
        throw SftpInitException{
            fmt::format("FATAL: Packet read of type {} instead of SSH_FXP_INIT", msg->type)};

    // Optional: Log the SSH_FXP_INIT reception like libssh does with SSH_LOG but with mp::log

    // sftp_reply_version isn't part of SftpMessage's interface: it's a one-off, non-public-API
    // libssh call used only for this handshake reply, so it stays on the raw message.
    if (MP_LIBSSH.sftp_reply_version(msg) != SSH_OK)
    {
        throw SftpInitException{"FATAL: Failed to process the SSH_FXP_INIT message"};
    }

    return raw_sftp_session;
}

mp::PlainSftpSession::PlainSftpSession(PlainSSHSession&& ssh_session_obj,
                                       const SftpClientSteward& client_steward,
                                       const std::string& source,
                                       const std::string& target)
    : plain_ssh_session{std::move(ssh_session_obj)},
      client_steward{client_steward},
      source{source},
      client_cmd{client_steward.compose_client_command(plain_ssh_session, source, target)}
{
    spawn_client();
}

mp::PlainSftpSession::~PlainSftpSession()
{
    raw_sftp_session.reset(); // mind the order: this borrows the process's channel
    client_process.reset();

    mp::top_catch_all(category,
                      [this] { client_steward.clean_up_after_client(plain_ssh_session, source); });
}

void mp::PlainSftpSession::spawn_client()
{
    assert(!client_process && "precondition - no client may be running");

    auto client_rollback = sg::make_scope_guard([this]() noexcept { client_process.reset(); });
    client_process = create_client_process(plain_ssh_session, client_cmd);
    raw_sftp_session = make_raw_sftp_session(plain_ssh_session.borrow_session(pass),
                                             client_process->borrow_channel(pass));
    client_rollback.dismiss();
}

void mp::PlainSftpSession::renew_client()
{
    if (!stop_requested.load())
    {
        mpl::debug(category, "Attempting SFTP client recovery.");

        raw_sftp_session.reset(); // mind the order: this borrows the process's channel
        client_process.reset();

        client_steward.clean_up_after_client(plain_ssh_session, source);
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
    while (!stop_requested.load() || !client_process)
    {
        int poll_result = poll_stdout(raw_sftp_session->channel, poll_interval.count());
        if (poll_result > 0) // bounded by session timeout
            raw_msg = MP_LIBSSH.sftp_get_client_message(raw_sftp_session.get());
        else if (poll_result == 0)
            continue; // nothing to read yet

        if (!raw_msg)
            mpl::debug(category,
                       "could not read the next client message: {}",
                       MP_LIBSSH.ssh_get_error(plain_ssh_session.borrow_session(pass)));

        break; // at this point, we either have a message or a connection drop/desync/malfunction
    }

    return raw_msg ? std::make_unique<PlainSftpMessage>(*raw_msg) : nullptr;
}

std::optional<int> mp::PlainSftpSession::client_exit_code()
{
    return client_process && client_process->exit_recognized(client_exit_timeout)
             ? std::optional{client_process->exit_code()}
             : std::nullopt;
}

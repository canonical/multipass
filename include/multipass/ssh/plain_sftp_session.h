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

#include <atomic>
#include <chrono>
#include <string>

struct sftp_session_struct;
struct ssh_channel_struct;
typedef ssh_channel_struct* ssh_channel;

namespace multipass
{
class SftpClientSteward;

/**
 * A concrete SftpSession backed by a remote SFTP client. It serves the SFTP protocol over an SSH
 * session to a client that is spawned on the guest, according to the specification provided by an
 * SftpClientSteward.
 */
class PlainSftpSession : public SftpSession, public PrivatePassProvider<PlainSftpSession>
{
public:
    /**
     * Interval at which #next_message polls for new messages while waiting for one to arrive.
     * Using int underneath: that is perfectly enough for intent and it is what libssh expects.
     */
    constexpr static std::chrono::duration<int, std::milli> poll_interval{250};

    /**
     * Consume an SSH session to serve SFTP to a remote client over it.
     *
     * @param ssh_session_obj The SSH session to serve on, which this consumes.
     * @see SSHSession::make_sftp_session for the semantics of the remaining params.
     */
    PlainSftpSession(PlainSSHSession&& ssh_session_obj,
                     const SftpClientSteward& client_steward,
                     const std::string& source,
                     const std::string& target);
    PlainSftpSession(const PlainSftpSession&) = delete;
    PlainSftpSession& operator=(const PlainSftpSession&) = delete;

    // Make class final before enabling these
    PlainSftpSession(PlainSftpSession&&) = delete;
    PlainSftpSession& operator=(PlainSftpSession&&) = delete;

    /**
     * @copydoc SftpSession::request_stop
     *
     * This will typically take up to #poll_interval to take effect, but it can take longer if:
     * @li reading a single SFTP message takes longer, e.g. because it arrives in chunks that are
     * slow to complete
     * @li the reading thread is delayed in being scheduled
     */
    void request_stop() noexcept override;

    std::unique_ptr<SftpMessage> next_message() override;

    /**
     * @copydoc SftpSession::renew_client
     *
     * This runs a new client in the guest, over the SSH session that this already holds.
     */
    void renew_client() override;

    /**
     * @copydoc SftpSession::client_failed
     *
     * This waits briefly for the client process to exit. When its exit status cannot be obtained
     * (e.g. timeout or SSH error), failure is assumed.
     */
    bool client_failed() override;

private:
    struct RawSftpSessionDeleter
    {
        void operator()(sftp_session_struct* session) const noexcept;
    };
    using RawSftpSessionUptr = std::unique_ptr<sftp_session_struct, RawSftpSessionDeleter>;

    static RawSftpSessionUptr make_raw_sftp_session(ssh_session raw_session, ssh_channel channel);

    /**
     * Run a new client in the guest and serve a new SFTP session to it.
     *
     * @pre No client of this session is currently running.
     */
    void spawn_client();

    PlainSSHSession plain_ssh_session;
    const SftpClientSteward& client_steward;
    const std::string source;
    const std::string client_cmd;
    std::unique_ptr<PlainSSHProcess> client_process;
    RawSftpSessionUptr raw_sftp_session;
    std::atomic<bool> stop_requested{false};
};
} // namespace multipass

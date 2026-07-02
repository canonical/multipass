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

struct sftp_session_struct;

namespace multipass
{

/**
 * A concrete SftpSession backed by an SSHFS mount: it serves the SFTP protocol over an SSH
 * session to a remote sshfs client, which mounts it on the guest.
 */
class PlainSftpSession : public SftpSession, public PrivatePassProvider<PlainSftpSession>
{
public:
    /**
     * Interval at which #next_message polls for new messages while waiting for one to arrive.
     * Using int underneath: that is perfectly enough for intent and it is what libssh expects.
     */
    constexpr static std::chrono::duration<int, std::milli> poll_interval{250};

    PlainSftpSession(PlainSSHSession&& ssh_session_obj, const std::string& sshfs_cmd);
    PlainSftpSession(const PlainSftpSession&) = delete;
    PlainSftpSession& operator=(const PlainSftpSession&) = delete;

    // TODO@sftp Make class final before enabling these
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

    /**
     * @copydoc SftpSession::next_message
     */
    std::unique_ptr<SftpMessage> next_message() override;

private:
    struct RawSftpSessionDeleter
    {
        void operator()(sftp_session_struct* message) const noexcept;
    };
    using RawSftpSessionUptr = std::unique_ptr<sftp_session_struct, RawSftpSessionDeleter>;

    static RawSftpSessionUptr make_raw_sftp_session(ssh_session raw_session, ssh_channel channel);

    PlainSSHSession plain_ssh_session;
    std::unique_ptr<PlainSSHProcess> sshfs_process;
    RawSftpSessionUptr raw_sftp_session;
    std::atomic<bool> stop_requested{false};
};
} // namespace multipass

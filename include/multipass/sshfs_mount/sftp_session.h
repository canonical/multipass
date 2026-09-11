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

#include <memory>
#include <optional>

namespace multipass
{
class SftpMessage;

/**
 * A local SFTP session, serving SFTP to a remote client.
 */
class SftpSession
{
public:
    virtual ~SftpSession() = default;

    // No copies
    SftpSession(const SftpSession&) = delete;
    SftpSession& operator=(const SftpSession&) = delete;

    /**
     * Request cooperative cancellation of this session.
     *
     * Call this method to cancel in-progress #next_message() calls on the same object (in other
     * threads) at the next occasion.
     */
    virtual void request_stop() noexcept = 0;

    /**
     * Poll for and return the next client message.
     *
     * Callers can tell apart the reasons for a `nullptr` return by checking whether they
     * themselves requested a stop.
     * @return The next message; `nullptr` if either:
     * @li #request_stop() was called; or
     * @li the connection ended or errored out
     */
    virtual std::unique_ptr<SftpMessage> next_message() = 0;

    /**
     * Re-establish the remote SFTP client.
     *
     * Call this method after #next_message() returned `nullptr` without a stop request and
     * #client_exit_code() ruled out a graceful end. The secure transport itself needs to be up:
     * this cannot recover from a broken connection.
     *
     * If a stop was requested with #request_stop() in the meantime, this is a no-op: no new
     * client is spawned; the stop request survives.
     *
     * @throws SSHException if renewing the client fails. The session is left in a valid
     * state with no client running; callers should either retry this method or destroy the
     * session.
     */
    virtual void renew_client() = 0;

    /**
     * Obtain the exit status of the remote client, if it has exited.
     *
     * Call this method after #next_message() returns `nullptr` without a stop request, to tell a
     * graceful end of the connection (the client unmounted and exited successfully) from any
     * other outcome.
     *
     * @return The client's exit code or `std::nullopt` if it could not be obtained
     * @throws SSHProcessExitError if the client's exit status cannot be obtained at all, meaning
     * the transport rather than the client is at fault (renewal will not help).
     */
    virtual std::optional<int> client_exit_code() = 0;

protected:
    SftpSession() = default;
};
} // namespace multipass

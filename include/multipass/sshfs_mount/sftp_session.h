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
     * #client_failed() confirmed the client at fault. The secure transport itself needs to be up:
     * this cannot recover from a broken connection.
     *
     * If a stop was requested with #request_stop() in the meantime, this is a no-op: no new
     * client is spawned; the stop request survives.
     */
    virtual void renew_client() = 0;

    /**
     * Tell whether the remote client failed.
     *
     * Call this method after #next_message() returns `nullptr` without a stop request, to
     * distinguish a client failure from a graceful end of the connection.
     *
     * @return True if the remote client terminated abnormally or its success could not be
     * confirmed; false if it exited successfully.
     */
    virtual bool client_failed() = 0;

protected:
    SftpSession() = default;
};
} // namespace multipass

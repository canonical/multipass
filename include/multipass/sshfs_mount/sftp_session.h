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
 * A server-side SFTP session.
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
    virtual void request_stop() = 0;

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

protected:
    SftpSession() = default;
};
} // namespace multipass

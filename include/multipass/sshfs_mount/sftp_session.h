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
     * Call this method to cancel in-progress @ref next_message() calls on the same object (in other
     * threads) at the next occasion.
     */
    virtual void request_stop() = 0;

    /**
     * Poll for and return the next client message.
     *
     * Returns `nullptr` either when the connection drops or when @ref request_stop() was called.
     * Callers can tell the two apart by checking whether they themselves requested a stop.
     */
    virtual std::unique_ptr<SftpMessage> next_message() = 0;

protected:
    SftpSession() = default;
};
} // namespace multipass

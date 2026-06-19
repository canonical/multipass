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

#include <multipass/ssh/ssh_process.h>

#include <libssh/libssh.h>

#include <memory>
#include <string>

namespace multipass
{
class SSHSession
{
public:
    virtual ~SSHSession() = default;

    /**
     * Execute a command in this SSH session.
     *
     * @pre !this->is_moved()
     * @note This promises no more than a conservative approach to thread-safety, whereby process
     * execution may be strictly sequenced, such that no two processes can execute simultaneously on
     * the same session. In other words, a process may execute only after any previous processes
     * have been destroyed or had exit_code called upon them.
     *
     * TODO@sftp make it such that exit_code releases only when the process exits (not on timeout).
     *
     * @param cmd The command to execute
     * @param whisper Whether to use trace rather than debug logging
     * @return An SSHProcess representing the remote process
     */
    virtual std::unique_ptr<SSHProcess> exec(const std::string& cmd, bool whisper = false) = 0;

    /**
     * @return Whether this object represents a session that is currently connected
     */
    [[nodiscard]] virtual bool is_connected() const = 0;

    /**
     * @return Whether this object has been moved from since the last assignment (or was last
     * assigned a moved object)
     */
    [[nodiscard]] virtual bool is_moved() const = 0;

    virtual operator ssh_session() = 0; // careful, not thread safe // TODO@sftp drop this?
    virtual void force_shutdown() = 0;  // careful, not thread safe

protected:
    SSHSession() = default;

    SSHSession(const SSHSession&) = delete;
    SSHSession& operator=(const SSHSession&) = delete;
    SSHSession(SSHSession&&) = default;
    SSHSession& operator=(SSHSession&&) = default;
};
} // namespace multipass

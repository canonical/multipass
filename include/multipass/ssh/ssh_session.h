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

    // locks the session until the process is destroyed or exit_code is called!
    virtual std::unique_ptr<SSHProcess> exec(const std::string& cmd, bool whisper = false) = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;

    virtual operator ssh_session() = 0; // careful, not thread safe // TODO@rewiressh drop this?
    virtual void force_shutdown() = 0;  // careful, not thread safe

protected:
    SSHSession() = default;

    SSHSession(const SSHSession&) = delete;
    SSHSession& operator=(const SSHSession&) = delete;
    SSHSession(SSHSession&&) = default;
    SSHSession& operator=(SSHSession&&) = default;
};
} // namespace multipass

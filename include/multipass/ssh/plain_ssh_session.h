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

#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/ssh_session.h>

#include <libssh/libssh.h>

#include <memory>
#include <mutex>
#include <string>

namespace multipass
{
class SSHKeyProvider;
class PlainSSHSession : public SSHSession
{
public:
    PlainSSHSession(const std::string& host,
                    int port,
                    const std::string& ssh_username,
                    const SSHKeyProvider& key_provider);

    // just being explicit (unique_ptr member already caused these to be deleted)
    PlainSSHSession(const PlainSSHSession&) = delete;
    PlainSSHSession& operator=(const PlainSSHSession&) = delete;

    // we should be able to move just fine though, but we need to lock
    PlainSSHSession(PlainSSHSession&&);
    PlainSSHSession& operator=(PlainSSHSession&&);

    ~PlainSSHSession() override;

    std::unique_ptr<SSHProcess> exec(const std::string& cmd, bool whisper = false) override;
    [[nodiscard]] bool is_connected() const override;

    operator ssh_session() override;
    void force_shutdown() override;

private:
    PlainSSHSession(PlainSSHSession&&, std::unique_lock<std::mutex> lock);

    void set_option(ssh_options_e type, const void* value);

    std::unique_ptr<ssh_session_struct, void (*)(ssh_session)> session;
    mutable std::mutex mut;
};
} // namespace multipass

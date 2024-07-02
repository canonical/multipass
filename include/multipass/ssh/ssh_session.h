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

#ifndef MULTIPASS_SSH_H
#define MULTIPASS_SSH_H

#include <multipass/ssh/ssh_process.h>

#include <libssh/libssh.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>

namespace multipass
{
class SSHKeyProvider;
class SSHSession
{
public:
    SSHSession(const std::string& host,
               int port,
               const std::string& ssh_username,
               const SSHKeyProvider& key_provider,
               const std::chrono::milliseconds timeout = std::chrono::seconds(20));

    // just being explicit (unique_ptr member already caused these to be deleted)
    SSHSession(const SSHSession&) = delete;
    SSHSession& operator=(const SSHSession&) = delete;

    // we should be able to move just fine though, but we need to lock
    SSHSession(SSHSession&&);
    SSHSession& operator=(SSHSession&&);

    ~SSHSession();

    SSHProcess exec(const std::string& cmd, bool whisper = false); /* locks the session until the process is destroyed
                                                                      or exit_code is called! */
    [[nodiscard]] bool is_connected() const;

    operator ssh_session(); // careful, not thread safe
    void force_shutdown();  // careful, not thread safe

private:
    SSHSession(SSHSession&&, std::unique_lock<std::mutex> lock);

    void set_option(ssh_options_e type, const void* value);

    std::unique_ptr<ssh_session_struct, void (*)(ssh_session)> session;
    mutable std::mutex mut;
};
} // namespace multipass
#endif // MULTIPASS_SSH_H

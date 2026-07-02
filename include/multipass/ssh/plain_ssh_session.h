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

#include <multipass/ssh/ssh_session.h>

#include <multipass/private_pass_provider.h>

#include <memory>
#include <mutex>
#include <string>

struct ssh_session_struct;

namespace multipass
{
class SSHKeyProvider;
class PlainSftpSession;
class PlainSSHProcess;

class PlainSSHSession final : public SSHSession // final to prevent chopping on move
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

    /**
     * @copydoc SSHSession::exec
     *
     * The dynamic type is always a PlainSSHProcess; see exec_plain to obtain it statically.
     */
    [[nodiscard]] std::unique_ptr<SSHProcess> exec(const std::string& cmd,
                                                   bool whisper = false) override;

    /**
     * TODO@sftp can we copydoc? partially
     * Like exec, but statically typed to the concrete PlainSSHProcess this session produces.
     */
    [[nodiscard]] std::unique_ptr<PlainSSHProcess> exec_plain(const std::string& cmd,
                                                              bool whisper = false);

    std::unique_ptr<SftpSession> make_sftp_session(const std::string& sshfs_cmd) && override;

    /**
     * @copydoc SSHSession::is_connected
     */
    [[nodiscard]] bool is_connected() const override;

    /**
     * @copydoc SSHSession::is_moved
     */
    [[nodiscard]] bool is_moved() const override;

    operator ssh_session() override; // TODO@sftp remove
    void shutdown_custom_socket() override; // TODO@sftp this should not be public

public: // but restricted
    /**
     * Obtain a non-owning libssh session handle.
     * The caller adopts thread-safety responsibility for the underlying session with respect to
     * this SSHSession
     *
     * @pre !this->is_moved()
     */
    ssh_session borrow_session(const PrivatePassProvider<PlainSftpSession>::PrivatePass&) const;

private:
    struct RawSSHSessionDeleter
    {
        void operator()(ssh_session_struct* message) const noexcept;
    };
    using RawSSHSessionUptr = std::unique_ptr<ssh_session_struct, RawSSHSessionDeleter>;

    PlainSSHSession(PlainSSHSession&&, std::unique_lock<std::mutex> lock);

    void set_option(ssh_options_e type, const void* value);

    RawSSHSessionUptr raw_session;
    mutable std::mutex mut;
};
} // namespace multipass

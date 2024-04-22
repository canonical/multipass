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

#ifndef MULTIPASS_SSH_PROCESS_H
#define MULTIPASS_SSH_PROCESS_H

#include <libssh/libssh.h>

#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <variant>

namespace multipass
{
class SSHProcess
{
public:
    using ChannelUPtr = std::unique_ptr<ssh_channel_struct, void (*)(ssh_channel)>;

    SSHProcess(ssh_session ssh_session, const std::string& cmd, std::unique_lock<std::mutex> session_lock);

    // just being explicit (unique_ptr member already caused these to be deleted)
    SSHProcess(const SSHProcess&) = delete;
    SSHProcess& operator=(const SSHProcess&) = delete;

    // we should be able to move just fine though
    SSHProcess(SSHProcess&&) = default;
    SSHProcess& operator=(SSHProcess&&) = default;

    ~SSHProcess() = default; // releases session lock

    // Attempt to verify process completion within the given timeout. For this to return true, two conditions are
    // necessary:
    //     a) the process did indeed finish;
    //     b) its exit code is read over ssh within the timeout.
    //
    // Note, in particular, that a false return does not guarantee that the process is still running. It may be just
    // that the exit code was not made available to us in a timely manner.
    //
    // This method caches the exit code if we find it, but it keeps the SSHSession locked.
    bool exit_recognized(std::chrono::milliseconds timeout = std::chrono::milliseconds(10)); // keeps session lock
    int exit_code(std::chrono::milliseconds timeout = std::chrono::seconds(5));              // releases session lock

    std::string read_std_output();
    std::string read_std_error();

private:
    enum class StreamType
    {
        out,
        err
    };

    void rethrow_if_saved() const;
    void read_exit_code(std::chrono::milliseconds timeout, bool save_exception);
    std::string read_stream(StreamType type, int timeout = -1);
    ssh_channel release_channel(); // releases the lock on the session; callers are on their own to ensure thread safety

    std::unique_lock<std::mutex> session_lock; // do not attempt to re-lock, as this is moved from
    ssh_session session;
    std::string cmd;
    ChannelUPtr channel;
    std::variant<std::monostate, int, std::exception_ptr> exit_result;

    friend class SftpServer;
};
} // namespace multipass
#endif // MULTIPASS_SSH_PROCESS_H

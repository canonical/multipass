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

#include <libssh/callbacks.h>
#include <libssh/libssh.h>

#include <memory>
#include <mutex>
#include <optional>

namespace multipass
{
class PlainSSHProcess : public SSHProcess
{
public:
    using ChannelUPtr = std::unique_ptr<ssh_channel_struct, void (*)(ssh_channel)>;
    using ExitResultType = std::optional<int>;
    using EventUPtr = std::unique_ptr<ssh_event_struct, void (*)(ssh_event)>;

    PlainSSHProcess(ssh_session_struct& ssh_session,
                    const std::string& cmd,
                    std::unique_lock<std::mutex> session_lock);

    ~PlainSSHProcess() override; // releases session lock

    // Attempt to verify process completion within the given timeout. For this to return true, two
    // conditions are necessary:
    //     a) the process did indeed finish;
    //     b) its exit code is read over ssh within the timeout.
    //
    // Note, in particular, that a false return does not guarantee that the process is still
    // running. It may be just that the exit code was not made available to us in a timely manner.
    //
    // This method caches the exit code if we find it, but it keeps the SSHSession locked.
    bool exit_recognized(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(10)) override;
    int exit_code(std::chrono::milliseconds timeout = std::chrono::seconds(5)) override;

    std::string read_std_output() override;
    std::string read_std_error() override;
    const std::string& get_cmd() const override;

private:
    enum class StreamType
    {
        out,
        err
    };

    void read_exit_code(std::chrono::milliseconds timeout);
    std::string read_stream(StreamType type, int timeout = -1);
    ssh_channel release_channel(); // releases the lock on the session; callers are on their own to
                                   // ensure thread safety
    EventUPtr get_event_in_session();

    static void channel_exit_status_cb(ssh_session session,
                                       ssh_channel channel,
                                       int exit_status,
                                       void* userdata);
    static void channel_exit_signal_cb(ssh_session session,
                                       ssh_channel channel,
                                       const char* signal,
                                       int core,
                                       const char* errmsg,
                                       const char* lang,
                                       void* userdata);
    static void channel_eof_cb(ssh_session session, ssh_channel channel, void* userdata);
    static void channel_close_cb(ssh_session session, ssh_channel channel, void* userdata);
    ssh_channel_callbacks_struct make_channel_callbacks();

    std::unique_lock<std::mutex> session_lock; // do not attempt to re-lock, as this is moved from
    ssh_session session;
    std::string cmd;

    ssh_channel_callbacks_struct cb;
    ChannelUPtr channel;

    ExitResultType exit_result{};
    bool channel_eof{false};
    bool channel_closed{false};

    friend class SftpServer;
};
} // namespace multipass

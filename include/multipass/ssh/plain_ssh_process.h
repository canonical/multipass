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

#include <functional>
#include <memory>
#include <mutex>

namespace multipass
{
class PlainSSHProcess : public SSHProcess
{
public:
    using ChannelUPtr = std::unique_ptr<ssh_channel_struct, void (*)(ssh_channel)>;
    using ExitCodeT = std::optional<int>;
    using EventUPtr = std::unique_ptr<ssh_event_struct, void (*)(ssh_event)>;

    PlainSSHProcess(ssh_session_struct& ssh_session,
                    const std::string& cmd,
                    std::unique_lock<std::mutex> session_lock);

    ~PlainSSHProcess() override; // releases session lock

    // The general contract is the following: when requesting callback data, if it is present it is
    // returned. If it is not present but the channel is closed, return null value (throw if
    // exit_code), if neither we poll. After polling, we either have the data (return), no data and
    // closed (throw if exit_code, null otherwise) or timeout(throw). Internal libssh errors always
    // throw.
    // Closed with no exit_code is abruptly closed session/channel, while closed without stdout or
    // stderr is left to the caller to interpret.

    int get_exit_code(sch::milliseconds timeout = sch::seconds(5)) override;

    std::string read_std_output(sch::milliseconds timeout = sch::seconds(1000)) override;
    std::string read_std_error(sch::milliseconds timeout = sch::seconds(1000)) override;
    const std::string& get_cmd() const override;

    // Attempt to verify process completion within the given timeout. For this to return true, two
    // conditions are necessary:
    //     a) the process did indeed finish;
    //     b) its exit code is read over ssh within the timeout.
    //
    // Note, in particular, that a false return does not guarantee that the process is still
    // running. It may be just that the exit code was not made available to us in a timely manner.
    //
    // This method caches the exit code if we find it, but it keeps the SSHSession locked.
    bool is_terminated(sch::milliseconds timeout = sch::milliseconds(10)) override;
    void close() override;

private:
    enum class StreamType
    {
        out,
        err
    };

    void read_exit_code(sch::milliseconds timeout);
    std::string read_stream(StreamType type, sch::milliseconds timeout);
    ssh_channel release_channel(); // releases the lock on the session; callers are on their own to
                                   // ensure thread safety
    EventUPtr get_event_in_session();
    // Poll the underlying socket until the timeout expires, the channel is closed or eof (no more
    // data incoming), an error happens or the predicate becomes true.
    int event_dopoll(sch::milliseconds timeout, std::function<bool()> while_condition);

    bool is_tmp_stream_empty(StreamType type) const noexcept;
    bool is_active() const noexcept;

    // Callbacks must always be noexcept!
    static int channel_data_cb(ssh_session session,
                               ssh_channel channel,
                               void* data,
                               uint32_t len,
                               int is_stderr,
                               void* userdata) noexcept;

    static void channel_exit_status_cb(ssh_session session,
                                       ssh_channel channel,
                                       int exit_status,
                                       void* userdata) noexcept;
    static void channel_exit_signal_cb(ssh_session session,
                                       ssh_channel channel,
                                       const char* signal,
                                       int core,
                                       const char* errmsg,
                                       const char* lang,
                                       void* userdata) noexcept;
    static void channel_eof_cb(ssh_session session, ssh_channel channel, void* userdata) noexcept;
    static void channel_close_cb(ssh_session session, ssh_channel channel, void* userdata) noexcept;
    ssh_channel_callbacks_struct make_channel_callbacks();

    std::unique_lock<std::mutex> session_lock; // do not attempt to re-lock, as this is moved from
    ssh_session session;
    std::string cmd;

    ssh_channel_callbacks_struct cb;
    ChannelUPtr channel;

    // Channel callback state
    ExitCodeT exit_code{};
    std::string stdout_buffer{};
    std::string stderr_buffer{};
    std::string tmp_stdout_buffer{};
    std::string tmp_stderr_buffer{};
    bool channel_eof{false};
    bool channel_closed{false};

    friend class SftpServer;
};
} // namespace multipass

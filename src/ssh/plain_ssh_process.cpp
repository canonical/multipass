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

#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log_location.h>
#include <multipass/platform.h>
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/top_catch_all.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "ssh process";

auto make_channel(ssh_session session, const std::string& cmd, ssh_channel_callbacks cb)
{
    if (!ssh_is_connected(session))
        throw mp::SSHException(fmt::format(
            "unable to create a channel for remote process: '{}', the SSH session is not connected",
            cmd));

    mp::PlainSSHProcess::ChannelUPtr channel{ssh_channel_new(session), ssh_channel_free};

    // Add callbacks before traffic is open
    mp::SSH::throw_on_error(channel,
                            session,
                            fmt::format("[{}] failed to add channel callbacks", category).c_str(),
                            ssh_add_channel_callbacks,
                            cb);
    mp::SSH::throw_on_error(channel,
                            session,
                            "[ssh proc] failed to open session channel",
                            ssh_channel_open_session);
    mp::SSH::throw_on_error(channel,
                            session,
                            "[ssh proc] exec request failed",
                            ssh_channel_request_exec,
                            cmd.c_str());
    // This class does not write to stdin
    mp::SSH::throw_on_error(channel, session, "[ssh proc] eof failed", ssh_channel_send_eof);
    return channel;
}

auto signal_to_exit_code(const char* sig)
{
    constexpr auto base_signal_code{128};
    std::string signal{sig};
    if (signal == "HUP")
        return base_signal_code + 1;
    if (signal == "INT")
        return base_signal_code + 2;
    if (signal == "QUIT")
        return base_signal_code + 3;
    if (signal == "ILL")
        return base_signal_code + 4;
    if (signal == "ABRT")
        return base_signal_code + 6;
    if (signal == "FPE")
        return base_signal_code + 8;
    if (signal == "KILL")
        return base_signal_code + 9;
    if (signal == "SEGV")
        return base_signal_code + 11;
    if (signal == "PIPE")
        return base_signal_code + 13;
    if (signal == "ALRM")
        return base_signal_code + 14;
    if (signal == "TERM")
        return base_signal_code + 15;
    return base_signal_code;
}

} // namespace

mp::PlainSSHProcess::PlainSSHProcess(ssh_session_struct& session,
                                     const std::string& cmd,
                                     std::unique_lock<std::mutex> session_lock)
    : session_lock{std::move(
          session_lock)}, // this is held until the exit code is requested or this is destroyed
      session{&session},
      cmd{cmd},
      cb{make_channel_callbacks()},
      channel{make_channel(this->session, cmd, &cb)}
{
    assert(this->session_lock.owns_lock());
}

mp::PlainSSHProcess::~PlainSSHProcess()
{
    if (channel) // TODO@sftp remove
        ssh_remove_channel_callbacks(channel.get(), &cb);
    // channel_free means forceful teardown if close() not called
}

ssh_channel_callbacks_struct mp::PlainSSHProcess::make_channel_callbacks()
{
    ssh_channel_callbacks_struct local_cb{};
    ssh_callbacks_init(&local_cb);
    local_cb.channel_exit_status_function = &PlainSSHProcess::channel_exit_status_cb;
    local_cb.channel_exit_signal_function = &PlainSSHProcess::channel_exit_signal_cb;
    local_cb.channel_eof_function = &PlainSSHProcess::channel_eof_cb;
    local_cb.channel_close_function = &PlainSSHProcess::channel_close_cb;
    local_cb.channel_data_function = &PlainSSHProcess::channel_data_cb;

    local_cb.userdata = this;
    return local_cb;
}

int mp::PlainSSHProcess::channel_data_cb(ssh_session,
                                         ssh_channel,
                                         void* data,
                                         uint32_t len,
                                         int is_stderr,
                                         void* userdata) noexcept
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    const char* char_ptr = static_cast<const char*>(data);

    mp::top_catch_all(category, [process, char_ptr, is_stderr, len]() {
        // Only safe to log inside a try catch block
        mpl::trace_location(category, "Received {} bytes on {}", len, is_stderr);

        if (is_stderr)
            process->tmp_stderr_buffer.append(char_ptr, len);
        else
            process->tmp_stdout_buffer.append(char_ptr, len);
    });
    return len;
}

void mp::PlainSSHProcess::channel_exit_status_cb(ssh_session,
                                                 ssh_channel,
                                                 int exit_status,
                                                 void* userdata) noexcept
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    process->exit_code = exit_status;
}

void mp::PlainSSHProcess::channel_exit_signal_cb(ssh_session,
                                                 ssh_channel,
                                                 const char* signal,
                                                 int core,
                                                 const char* errmsg,
                                                 const char*,
                                                 void* userdata) noexcept
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    mp::top_catch_all(category, [process, signal, core, errmsg] {
        mpl::error(category,
                   "{}: SIG{}, {} {}",
                   process->cmd,
                   signal ? signal : "UNKNOWN",
                   errmsg,
                   core ? "(core dumped)" : "");

        auto sig_code{signal_to_exit_code(signal)};
        process->exit_code = sig_code;
    });
}

void mp::PlainSSHProcess::channel_eof_cb(ssh_session, ssh_channel, void* userdata) noexcept
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    process->channel_eof = true;
}

void mp::PlainSSHProcess::channel_close_cb(ssh_session, ssh_channel, void* userdata) noexcept
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    process->channel_closed = true;
    process->channel_eof = true;
}

int mp::PlainSSHProcess::get_exit_code(sch::milliseconds timeout)
{
    if (exit_code.has_value())
        return exit_code.value();
    else if (channel_closed)
        throw SSHProcessExitError{cmd, "channel is closed with no exit status"};

    auto local_lock = std::move(session_lock); // unlock at the end
    read_exit_code(timeout);

    assert(exit_code.has_value()); // All other possibilities are covered by exceptions
    return exit_code.value();
}

void mp::PlainSSHProcess::read_exit_code(sch::milliseconds timeout)
{
    if (!channel) // TODO@sftp remove check
        throw SSHProcessExitError{cmd, "channel is null"};

    const auto while_condition = [this] { return !this->exit_code.has_value(); };

    int rc{event_dopoll(timeout, while_condition)};

    if (while_condition())
    {
        if (rc == SSH_EOF)
            throw SSHProcessExitError{cmd, "channel is closed with no exit status"};
        if (rc == SSH_ERROR && channel_closed)
            // This exception means no more data can be obtained from this channel.
            throw SSHAbruptlyClosed{"{}: channel abruptly closed", cmd};
        else
            // SSH_ERROR without closed means EINTR signal (throw otherwise), we should try again
            // (timeout) SSH_AGAIN is timeout SSH_OK is not possible
            throw SSHProcessTimeoutException{cmd, timeout};
    }
}

std::string mp::PlainSSHProcess::read_std_output(sch::milliseconds timeout)
{
    return read_stream(StreamType::out, timeout);
}

std::string mp::PlainSSHProcess::read_std_error(sch::milliseconds timeout)
{
    return read_stream(StreamType::err, timeout);
}

const std::string& mp::PlainSSHProcess::get_cmd() const
{
    return cmd;
}

std::string mp::PlainSSHProcess::read_stream(StreamType type, sch::milliseconds timeout)
{
    mpl::trace_location(category, "(type = {}, timeout = {})", static_cast<int>(type), timeout);
    // Slight variation of the contract: we poll even if we have data because there could be more in
    // the socket
    auto starting_time = sch::steady_clock::now();

    std::string* persistent_stream = &stdout_buffer;
    std::string* staging_stream = &tmp_stdout_buffer;

    if (type == StreamType::err)
    {
        persistent_stream = &stderr_buffer;
        staging_stream = &tmp_stderr_buffer;
    }
    auto while_condition = [this, type] { return this->is_tmp_stream_empty(type); };

    auto deadline = starting_time + timeout;
    while (!channel_eof && (sch::steady_clock::now() < deadline))
    {
        auto current_timeout =
            std::min(sch::duration_cast<sch::milliseconds>(deadline - sch::steady_clock::now()),
                     sch::milliseconds(250));
        int rc{event_dopoll(current_timeout, while_condition)};
        if (while_condition())
            if (rc == SSH_EOF || (rc == SSH_ERROR && channel_closed)) // No more data
                break;
            else // SSH_AGAIN or SSH_ERROR + EINTR
                continue;
        else
        {
            persistent_stream->append(*staging_stream);
            staging_stream->clear();
        }
    }
    std::string output{std::move(*persistent_stream)};
    persistent_stream->clear();
    return output;
}

ssh_channel mp::PlainSSHProcess::release_channel()
{
    // released at the end; callers are on their own to ensure thread safety
    auto local_lock = std::move(session_lock);
    return channel.release();
}

mp::PlainSSHProcess::EventUPtr mp::PlainSSHProcess::get_event_in_session()
{
    mp::PlainSSHProcess::EventUPtr event{ssh_event_new(), ssh_event_free};

    std::optional<std::string> err = std::nullopt;
    if (!event)
        err = "could not allocate event";
    else if ((ssh_event_add_session(event.get(), session) != SSH_OK))
    {
        const auto raw_err = ssh_get_error(session);
        err = fmt::format("could not add event to session: {}",
                          raw_err && *raw_err ? raw_err : "Empty error");
    }
    if (err.has_value())
        throw SSHException{fmt::format("{}: {}", cmd, err.value())};

    return event;
}

int mp::PlainSSHProcess::event_dopoll(sch::milliseconds timeout,
                                      std::function<bool()> while_condition)
{
    // Contract: Function should not be called if the channel is inactive or while_condition is
    // false
    assert(is_active() && while_condition());
    auto starting_time = sch::steady_clock::now();
    auto event = get_event_in_session();

    auto deadline = starting_time + timeout;

    int rc{SSH_OK};
    while ((sch::steady_clock::now() < deadline) && rc != SSH_ERROR && while_condition() &&
           is_active())
    {
        auto current_timeout = deadline - sch::steady_clock::now();
        rc = ssh_event_dopoll(event.get(), current_timeout.count());
    }

    if (while_condition())
    {
        if (rc == SSH_ERROR)
        {
            // SSH_ERROR with no closed or eof means that ::poll returned <0. Further polling will
            // fail (unless the error was EINTR).
            auto local_errno{MP_PLATFORM.get_errno()};
            if (local_errno != EINTR)
            {
                channel_closed = true;
                channel_eof = true;
            }
        }
        else if (rc == SSH_OK && !is_active())
            // Channel closed gracefully without filling condition
            // Signal with separate code
            rc = SSH_EOF;
        else
            // SSH_AGAIN or chrono timeout
            rc = SSH_AGAIN;
    }
    return rc;
}

bool mp::PlainSSHProcess::is_terminated(sch::milliseconds timeout)
{
    if (!is_active())
        return true;
    event_dopoll(timeout, [this] { return this->is_active(); });
    return !is_active();
}

bool mp::PlainSSHProcess::is_active() const noexcept
{
    // EOF means that there will be no more stdout/err, but could still receive the exit_code, and
    // exit_code means the process has exited, but there could still be stdout/err packets to read
    return !(channel_closed || (channel_eof && exit_code.has_value()));
}

bool mp::PlainSSHProcess::is_tmp_stream_empty(mp::PlainSSHProcess::StreamType type) const noexcept
{
    switch (type)
    {
    case mp::PlainSSHProcess::StreamType::err:
        return tmp_stderr_buffer.empty();
    case mp::PlainSSHProcess::StreamType::out:
        return tmp_stdout_buffer.empty();
    }
    assert(false && "unreachable");
}

void mp::PlainSSHProcess::close()
{
    // If the channel is already null or closed, there's nothing to do
    if (!channel || channel_closed) // TODO@sftp remove channel check
    {
        mpl::trace_location(category, "Channel already closed or null for command: {}", cmd);
        return;
    }
    mpl::trace_location(category, "Initiating graceful blocking close handshake for: {}", cmd);

    // This blocks the thread until the SSH2_MSG_CHANNEL_CLOSE handshake completes
    int rc = ssh_channel_close(channel.get());

    if (rc != SSH_OK)
        mpl::error(category,
                   "Graceful blocking close failed for command '{}'. libssh code: {}",
                   cmd,
                   rc);
    else
        mpl::trace_location(category,
                            "Graceful close handshake finished successfully for: {}",
                            cmd);
}

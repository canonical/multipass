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
#include <multipass/ssh/libssh_wrapper.h>
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/throw_on_error.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <functional>
#include <sstream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "ssh process";

auto make_channel(ssh_session session, const std::string& cmd, ssh_channel_callbacks cb)
{
    if (!MP_LIBSSH.ssh_is_connected(session))
        throw mp::SSHException(fmt::format(
            "unable to create a channel for remote process: '{}', the SSH session is not connected",
            cmd));

    mp::PlainSSHProcess::ChannelUPtr channel{
        MP_LIBSSH.ssh_channel_new(session),
        [](ssh_channel ch) { MP_LIBSSH.ssh_channel_free(ch); }};
    // Add callbacks before traffic is open
    mp::SSH::throw_on_error(
        channel,
        session,
        fmt::format("[{}] failed to add channel callbacks", category).c_str(),
        std::bind_front(&mp::Libssh::ssh_add_channel_callbacks, &mp::Libssh::instance()),
        cb);
    mp::SSH::throw_on_error(
        channel,
        session,
        "[ssh proc] failed to open session channel",
        std::bind_front(&mp::Libssh::ssh_channel_open_session, &mp::Libssh::instance()));
    mp::SSH::throw_on_error(
        channel,
        session,
        "[ssh proc] exec request failed",
        std::bind_front(&mp::Libssh::ssh_channel_request_exec, &mp::Libssh::instance()),
        cmd.c_str());
    return channel;
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
}

ssh_channel_callbacks_struct mp::PlainSSHProcess::make_channel_callbacks()
{
    ssh_channel_callbacks_struct local_cb{};
    ssh_callbacks_init(&local_cb);
    local_cb.channel_exit_status_function = &PlainSSHProcess::channel_exit_status_cb;
    local_cb.channel_exit_signal_function = &PlainSSHProcess::channel_exit_signal_cb;
    local_cb.channel_eof_function = &PlainSSHProcess::channel_eof_cb;
    local_cb.channel_close_function = &PlainSSHProcess::channel_close_cb;

    local_cb.userdata = this;
    return local_cb;
}

void mp::PlainSSHProcess::channel_exit_status_cb(ssh_session,
                                                 ssh_channel,
                                                 int exit_status,
                                                 void* userdata)
{
    auto* process = reinterpret_cast<mp::PlainSSHProcess*>(userdata);
    process->exit_result = exit_status;
}

void mp::PlainSSHProcess::channel_exit_signal_cb(ssh_session,
                                                 ssh_channel,
                                                 const char* signal,
                                                 int,
                                                 const char*,
                                                 const char*,
                                                 void* userdata)
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    std::string sig_name = signal ? signal : "UNKNOWN";

    process->exit_result = std::make_exception_ptr(
        SSHProcessExitError{process->cmd, "Process terminated by remote signal: SIG" + sig_name});
}

void mp::PlainSSHProcess::channel_eof_cb(ssh_session, ssh_channel, void* userdata)
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    process->remote_eof = true;
}

void mp::PlainSSHProcess::channel_close_cb(ssh_session, ssh_channel, void* userdata)
{
    auto* process = static_cast<mp::PlainSSHProcess*>(userdata);
    process->remote_closed = true;
    process->remote_eof = true;
}

bool mp::PlainSSHProcess::exit_recognized(std::chrono::milliseconds timeout)
{
    rethrow_if_saved();
    if (std::holds_alternative<int>(exit_result))
        return true;

    try
    {
        read_exit_code(timeout, /* save_exception = */ false);
        return true;
    }
    catch (SSHProcessTimeoutException&)
    {
        return false;
    }
}

int mp::PlainSSHProcess::exit_code(std::chrono::milliseconds timeout)
{
    rethrow_if_saved();
    if (auto exit_status = std::get_if<int>(&exit_result))
        return *exit_status;

    auto local_lock = std::move(session_lock); // unlock at the end
    read_exit_code(timeout, /* save_exception = */ true);

    assert(std::holds_alternative<int>(exit_result));
    return std::get<int>(exit_result);
}

void mp::PlainSSHProcess::read_exit_code(std::chrono::milliseconds timeout, bool save_exception)
{
    assert(std::holds_alternative<std::monostate>(exit_result));
    std::exception_ptr eptr;
    if (!channel) // TODO@sftp remove check
    {
        eptr = std::make_exception_ptr(SSHProcessExitError{cmd, "channel is null"});

        if (save_exception)
            exit_result = eptr;

        std::rethrow_exception(eptr);
    }

    auto event = get_event_in_session(save_exception);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    int rc{SSH_OK};
    while ((std::chrono::steady_clock::now() < deadline) && rc == SSH_OK &&
           !std::holds_alternative<int>(exit_result))
    {
        rc = MP_LIBSSH.ssh_event_dopoll(event.get(), timeout.count());
    }

    if (!std::holds_alternative<int>(exit_result))
    {
        if (rc == SSH_ERROR)
        { // we expect SSH_AGAIN or SSH_OK (unchanged) when there is a timeout
            const auto err = fmt::format("ssh_event_dopoll failed: {}", std::strerror(errno));
            eptr = std::make_exception_ptr(SSHProcessExitError{cmd, err});
        }
        else
            eptr = std::make_exception_ptr(SSHProcessTimeoutException{cmd, timeout});
        // note that make_exception_ptr takes by value; we repeat the call with the concrete
        // types to avoid slicing

        if (save_exception)
            exit_result = eptr;

        std::rethrow_exception(eptr);
    }
}

std::string mp::PlainSSHProcess::read_std_output()
{
    return read_stream(StreamType::out);
}

std::string mp::PlainSSHProcess::read_std_error()
{
    return read_stream(StreamType::err);
}

const std::string& mp::PlainSSHProcess::get_cmd() const
{
    return cmd;
}

std::string mp::PlainSSHProcess::read_stream(StreamType type, int timeout)
{
    mpl::trace_location(category, "(type = {}, timeout = {})", static_cast<int>(type), timeout);

    // If the channel is closed there's no output to read
    if (!channel || MP_LIBSSH.ssh_channel_is_closed(channel.get())) // TODO@sftp
    {
        mpl::trace_location(category, "{}", !channel ? "null channel" : "channel closed");
        return std::string();
    }

    std::stringstream output;
    std::array<char, 256> buffer;
    int num_bytes{0};
    const bool is_std_err = type == StreamType::err;
    do
    {
        num_bytes = MP_LIBSSH.ssh_channel_read_timeout(channel.get(),
                                                       buffer.data(),
                                                       buffer.size(),
                                                       is_std_err,
                                                       timeout);
        mpl::trace_location(category, "num_bytes = {}", num_bytes);
        if (num_bytes < 0)
        {
            // Latest libssh now returns an error if the channel has been closed instead of
            // returning 0 bytes
            if (MP_LIBSSH.ssh_channel_is_closed(channel.get()))
            {
                mpl::trace_location(category, "channel closed");
                return output.str();
            }

            throw mp::SSHException(
                fmt::format("error while reading ssh channel for remote process '{}' - error: {}",
                            cmd,
                            num_bytes));
        }
        output.write(buffer.data(), num_bytes);
    } while (num_bytes > 0);

    return output.str();
}

ssh_channel mp::PlainSSHProcess::release_channel()
{
    // released at the end; callers are on their own to ensure thread safety
    auto local_lock = std::move(session_lock);
    return channel.release();
}

void multipass::PlainSSHProcess::rethrow_if_saved() const
{
    if (auto eptrptr = std::get_if<std::exception_ptr>(&exit_result))
    {
        assert(!session_lock.owns_lock());
        std::rethrow_exception(*eptrptr);
    }
}

mp::PlainSSHProcess::EventUPtr mp::PlainSSHProcess::get_event_in_session(bool save_exception)
{

    mp::PlainSSHProcess::EventUPtr event{MP_LIBSSH.ssh_event_new(),
                                         [](ssh_event e) { MP_LIBSSH.ssh_event_free(e); }};

    std::optional<std::string> err = std::nullopt;
    if (!event)
        err = "could not allocate event";
    else if (MP_LIBSSH.ssh_event_add_session(event.get(), session) != SSH_OK)
    {
        const auto raw_err = ssh_get_error(session);
        err = fmt::format("could not add event to session: {}",
                          raw_err && *raw_err ? raw_err : "Empty error");
    }
    if (err.has_value())
    {
        std::exception_ptr eptr = std::make_exception_ptr(SSHProcessExitError{cmd, err.value()});
        if (save_exception)
            exit_result = eptr;
        std::rethrow_exception(eptr);
    }
    return event;
}

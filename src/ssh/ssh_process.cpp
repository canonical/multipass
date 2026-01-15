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
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_process.h>
#include <multipass/ssh/throw_on_error.h>

#include <libssh/callbacks.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <source_location>
#include <sstream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "ssh process";

constexpr std::string_view extract_filename(std::string_view path)
{
    assert(path.empty() || (*path.rbegin() != '/' && *path.rbegin() != '\\')); // no trailing slash

    auto pos = path.find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

template <typename... Args>
struct trace_loc
{
    trace_loc(fmt::format_string<Args...> fmt,
              Args&&... args,
              std::source_location location = std::source_location::current())
    {
        mpl::trace(category,
                   "{}:{} {}(): {}",
                   extract_filename(location.file_name()),
                   location.line(),
                   location.function_name(),
                   fmt::format(fmt, std::forward<Args>(args)...));
    }
};

template <typename... Args>
trace_loc(fmt::format_string<Args...>, Args&&...) -> trace_loc<Args...>;

template <typename T>
class ExitStatusCallback
{
public:
    ExitStatusCallback(ssh_channel channel, T& result_holder) : channel{channel}
    {
        ssh_callbacks_init(&cb);
        cb.channel_exit_status_function = channel_exit_status_cb;
        cb.userdata = &result_holder;
        ssh_add_channel_callbacks(channel, &cb);
    }
    ~ExitStatusCallback()
    {
        ssh_remove_channel_callbacks(channel, &cb);
    }

private:
    static void channel_exit_status_cb(ssh_session, ssh_channel, int exit_status, void* userdata)
    {
        auto exit_code = reinterpret_cast<T*>(userdata);
        *exit_code = exit_status;
    }
    ssh_channel channel;
    ssh_channel_callbacks_struct cb{};
};

auto make_channel(ssh_session session, const std::string& cmd)
{
    if (!ssh_is_connected(session))
    {
        trace_loc("unable to create a channel for remote process: '{}', SSH session not connected",
                  cmd);

        throw mp::SSHException(fmt::format(
            "unable to create a channel for remote process: '{}', the SSH session is not connected",
            cmd));
    }
    mp::SSHProcess::ChannelUPtr channel{ssh_channel_new(session), ssh_channel_free};
    trace_loc("[ssh proc] opening session channel");
    mp::SSH::throw_on_error(channel,
                            session,
                            "[ssh proc] failed to open session channel",
                            ssh_channel_open_session);
    trace_loc("[ssh proc] executing command: '{}'", cmd);
    mp::SSH::throw_on_error(channel,
                            session,
                            "[ssh proc] exec request failed",
                            ssh_channel_request_exec,
                            cmd.c_str());
    return channel;
}

} // namespace

mp::SSHProcess::SSHProcess(ssh_session session,
                           const std::string& cmd,
                           std::unique_lock<std::mutex> session_lock)
    : session_lock{std::move(
          session_lock)}, // this is held until the exit code is requested or this is destroyed
      session{session},
      cmd{cmd},
      channel{make_channel(session, cmd)},
      exit_result{}
{
    assert(this->session_lock.owns_lock());
}

bool mp::SSHProcess::exit_recognized(std::chrono::milliseconds timeout)
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

int mp::SSHProcess::exit_code(std::chrono::milliseconds timeout)
{
    rethrow_if_saved();
    if (auto exit_status = std::get_if<int>(&exit_result))
        return *exit_status;

    auto local_lock = std::move(session_lock); // unlock at the end
    read_exit_code(timeout, /* save_exception = */ true);

    assert(std::holds_alternative<int>(exit_result));
    return std::get<int>(exit_result);
}

void mp::SSHProcess::read_exit_code(std::chrono::milliseconds timeout, bool save_exception)
{
    assert(std::holds_alternative<std::monostate>(exit_result));
    ExitStatusCallback cb{channel.get(), exit_result};

    std::unique_ptr<ssh_event_struct, decltype(ssh_event_free)*> event{ssh_event_new(),
                                                                       ssh_event_free};
    ssh_event_add_session(event.get(), session);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    int rc{SSH_OK};
    while ((std::chrono::steady_clock::now() < deadline) && rc == SSH_OK &&
           !std::holds_alternative<int>(exit_result))
    {
        rc = ssh_event_dopoll(event.get(), timeout.count());
    }

    if (!std::holds_alternative<int>(exit_result))
    {
        if (rc == SSH_ERROR) // we expect SSH_AGAIN or SSH_OK (unchanged) when there is a timeout
            trace_loc("SSHProcessExitError for cmd '{}': {}", cmd, std::strerror(errno));
        else
            trace_loc("SSHProcessTimeoutException for cmd '{}', timeout: {}", cmd, timeout);
        std::exception_ptr eptr;
         if (rc == SSH_ERROR)
            eptr = std::make_exception_ptr(SSHProcessExitError{cmd, std::strerror(errno)});
        else
            eptr = std::make_exception_ptr(SSHProcessTimeoutException{cmd, timeout});
        // note that make_exception_ptr takes by value; we repeat the call with the concrete types
        // to avoid slicing

        if (save_exception)
            exit_result = eptr;

        rethrow_exception(eptr);
    }
}

std::string mp::SSHProcess::read_std_output()
{
    return read_stream(StreamType::out);
}

std::string mp::SSHProcess::read_std_error()
{
    return read_stream(StreamType::err);
}

std::string mp::SSHProcess::read_stream(StreamType type, int timeout)
{
    trace_loc("(type = {}, timeout = {})", static_cast<int>(type), timeout);

    // If the channel is closed there's no output to read
    if (ssh_channel_is_closed(channel.get()))
    {
        trace_loc("channel closed");
        return std::string();
    }

    std::stringstream output;
    std::array<char, 256> buffer;
    int num_bytes{0};
    const bool is_std_err = type == StreamType::err;
    do
    {
        num_bytes = ssh_channel_read_timeout(channel.get(),
                                             buffer.data(),
                                             buffer.size(),
                                             is_std_err,
                                             timeout);
        trace_loc("num_bytes = {}", num_bytes);
        if (num_bytes < 0)
        {
            // Latest libssh now returns an error if the channel has been closed instead of
            // returning 0 bytes
            if (ssh_channel_is_closed(channel.get()))
            {
                trace_loc("channel closed");
                return output.str();
            }
            trace_loc("SSH read error for cmd '{}', error: {}", cmd, num_bytes);
            throw mp::SSHException(
                fmt::format("error while reading ssh channel for remote process '{}' - error: {}",
                            cmd,
                            num_bytes));
        }
        output.write(buffer.data(), num_bytes);
    } while (num_bytes > 0);

    return output.str();
}

ssh_channel mp::SSHProcess::release_channel()
{
    auto local_lock = std::move(
        session_lock); // released at the end; callers are on their own to ensure thread safety
    return channel.release();
}

void multipass::SSHProcess::rethrow_if_saved() const
{
    if (auto eptrptr = std::get_if<std::exception_ptr>(&exit_result))
    {
        assert(!session_lock.owns_lock());
        std::rethrow_exception(*eptrptr);
    }
}

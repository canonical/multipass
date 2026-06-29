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
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/throw_on_error.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "ssh process";

static void channel_exit_status_cb(ssh_session, ssh_channel, int exit_status, void* userdata)
{
    auto exit_code = static_cast<mp::PlainSSHProcess::ExitResultType*>(userdata);
    *exit_code = exit_status;
}

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
    return channel;
}

auto make_channel_callbacks(mp::PlainSSHProcess::ExitResultType& exit_result)
{
    ssh_channel_callbacks_struct local_cb{};
    ssh_callbacks_init(&local_cb);
    local_cb.channel_exit_status_function = channel_exit_status_cb;
    local_cb.userdata = &exit_result;
    return local_cb;
}

} // namespace

mp::PlainSSHProcess::PlainSSHProcess(ssh_session_struct& session,
                                     const std::string& cmd,
                                     std::unique_lock<std::mutex> session_lock)
    : session_lock{std::move(
          session_lock)}, // this is held until the exit code is requested or this is destroyed
      session{&session},
      cmd{cmd},
      cb{make_channel_callbacks(exit_result)},
      channel{make_channel(this->session, cmd, &cb)}
{
    assert(this->session_lock.owns_lock());
}

mp::PlainSSHProcess::~PlainSSHProcess()
{
    if (channel) // TODO@sftp remove
        ssh_remove_channel_callbacks(channel.get(), &cb);
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
    std::unique_ptr<ssh_event_struct, decltype(ssh_event_free)*> event{ssh_event_new(),
                                                                       ssh_event_free};

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
    {
        eptr = std::make_exception_ptr(SSHProcessExitError{cmd, err.value()});
        if (save_exception)
            exit_result = eptr;
        std::rethrow_exception(eptr);
    }

    auto deadline = std::chrono::steady_clock::now() + timeout;

    int rc{SSH_OK};
    while ((std::chrono::steady_clock::now() < deadline) && rc == SSH_OK &&
           !std::holds_alternative<int>(exit_result))
    {
        rc = ssh_event_dopoll(event.get(), timeout.count());
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
    if (!channel || ssh_channel_is_closed(channel.get())) // TODO@sftp
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
        num_bytes = ssh_channel_read_timeout(channel.get(),
                                             buffer.data(),
                                             buffer.size(),
                                             is_std_err,
                                             timeout);
        mpl::trace_location(category, "num_bytes = {}", num_bytes);
        if (num_bytes < 0)
        {
            // Latest libssh now returns an error if the channel has been closed instead of
            // returning 0 bytes
            if (ssh_channel_is_closed(channel.get()))
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

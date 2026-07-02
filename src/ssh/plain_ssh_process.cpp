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

#include <libssh/callbacks.h>

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

template <typename T>
class ExitStatusCallback
{
public:
    ExitStatusCallback(ssh_channel channel, T& result_holder) : channel{channel}
    {
        ssh_callbacks_init(&cb);
        cb.channel_exit_status_function = channel_exit_status_cb;
        cb.userdata = &result_holder;
        registered = MP_LIBSSH.ssh_add_channel_callbacks(channel, &cb);
    }
    ~ExitStatusCallback()
    {
        if (is_registered())
            MP_LIBSSH.ssh_remove_channel_callbacks(channel, &cb);
    }
    bool is_registered() const
    {
        return registered == SSH_OK;
    }

private:
    static void channel_exit_status_cb(ssh_session, ssh_channel, int exit_status, void* userdata)
    {
        auto exit_code = reinterpret_cast<T*>(userdata);
        *exit_code = exit_status;
    }
    ssh_channel channel;
    ssh_channel_callbacks_struct cb{};
    int registered{};
};

} // namespace

void mp::PlainSSHProcess::ChannelDeleter::operator()(ssh_channel_struct* channel) const noexcept
{
    MP_LIBSSH.ssh_channel_free(channel);
}

mp::PlainSSHProcess::ChannelUPtr mp::PlainSSHProcess::make_channel(ssh_session raw_session,
                                                                   const std::string& cmd)
{
    if (!MP_LIBSSH.ssh_is_connected(raw_session))
        throw mp::SSHException(fmt::format(
            "unable to create a channel for remote process: '{}', the SSH session is not connected",
            cmd));

    ChannelUPtr channel{
        MP_LIBSSH.ssh_channel_new(raw_session)};
    SSH::throw_on_error(
        channel,
        raw_session,
        "[ssh proc] failed to open session channel",
        std::bind_front(&Libssh::ssh_channel_open_session, &Libssh::instance()));
    SSH::throw_on_error(
        channel,
        raw_session,
        "[ssh proc] exec request failed",
        std::bind_front(&Libssh::ssh_channel_request_exec, &Libssh::instance()),
        cmd.c_str());
    return channel;
}

mp::PlainSSHProcess::PlainSSHProcess(ssh_session_struct& raw_session,
                                     const std::string& cmd,
                                     std::unique_lock<std::mutex> session_lock)
    : session_lock{std::move(
          session_lock)}, // this is held until the exit code is requested or this is destroyed
      raw_session{&raw_session},
      cmd{cmd},
      channel{make_channel(this->raw_session, cmd)},
      exit_result{}
{
    assert(this->session_lock.owns_lock());
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
    ExitStatusCallback cb{channel.get(), exit_result};
    std::unique_ptr<ssh_event_struct, void (*)(ssh_event)> event{
        MP_LIBSSH.ssh_event_new(),
        [](ssh_event e) { MP_LIBSSH.ssh_event_free(e); }};

    std::optional<std::string> err = std::nullopt;
    if (!event)
        err = "could not allocate event";
    else if (!cb.is_registered())
        err = "could not register callback";
    else if ((MP_LIBSSH.ssh_event_add_session(event.get(), raw_session) != SSH_OK))
    {
        const auto raw_err = MP_LIBSSH.ssh_get_error(raw_session);
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

ssh_channel mp::PlainSSHProcess::borrow_channel(
    const PrivatePassProvider<PlainSftpSession>::PrivatePass&)
{
    auto local_lock = std::move(session_lock); // released at end; caller gets a non-owning handle
    return channel.get();
}

ssh_channel mp::PlainSSHProcess::release_channel() // TODO@sftp remove entirely
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

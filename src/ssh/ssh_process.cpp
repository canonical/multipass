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

#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_process.h>
#include <multipass/ssh/throw_on_error.h>

#include <libssh/callbacks.h>

#include <array>
#include <sstream>

#include <cerrno>
#include <cstring>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "ssh process";

class ExitStatusCallback
{
public:
    ExitStatusCallback(ssh_channel channel, std::optional<int>& exit_status) : channel{channel}
    {
        ssh_callbacks_init(&cb);
        cb.channel_exit_status_function = channel_exit_status_cb;
        cb.userdata = &exit_status;
        ssh_add_channel_callbacks(channel, &cb);
    }
    ~ExitStatusCallback()
    {
        ssh_remove_channel_callbacks(channel, &cb);
    }

private:
    static void channel_exit_status_cb(ssh_session, ssh_channel, int exit_status, void* userdata)
    {
        auto exit_code = reinterpret_cast<std::optional<int>*>(userdata);
        *exit_code = exit_status;
    }
    ssh_channel channel;
    ssh_channel_callbacks_struct cb{};
};

auto make_channel(ssh_session session, const std::string& cmd)
{
    if (!ssh_is_connected(session))
        throw mp::SSHException(
            fmt::format("unable to create a channel for remote process: '{}', the SSH session is not connected", cmd));

    mp::SSHProcess::ChannelUPtr channel{ssh_channel_new(session), ssh_channel_free};
    mp::SSH::throw_on_error(channel, session, "[ssh proc] failed to open session channel", ssh_channel_open_session);
    mp::SSH::throw_on_error(channel, session, "[ssh proc] exec request failed", ssh_channel_request_exec, cmd.c_str());
    return channel;
}
} // namespace

mp::SSHProcess::SSHProcess(ssh_session session, const std::string& cmd)
    : session{session}, cmd{cmd}, channel{make_channel(session, cmd)}
{
}

int mp::SSHProcess::exit_code(std::chrono::milliseconds timeout)
{
    exit_status = std::nullopt;
    ExitStatusCallback cb{channel.get(), exit_status};

    std::unique_ptr<ssh_event_struct, decltype(ssh_event_free)*> event{ssh_event_new(), ssh_event_free};
    ssh_event_add_session(event.get(), session);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    int rc{SSH_OK};
    while ((std::chrono::steady_clock::now() < deadline) && rc == SSH_OK && !exit_status)
    {
        rc = ssh_event_dopoll(event.get(), timeout.count());
    }

    if (!exit_status) // we expect SSH_AGAIN or SSH_OK (unchanged) when there is a timeout
        throw ExitlessSSHProcessException{cmd, rc == SSH_ERROR ? std::strerror(errno) : "timeout"};

    return *exit_status;
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
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(type = {}, timeout = {}): ", __FILE__, __LINE__, __FUNCTION__,
                         static_cast<int>(type), timeout));
    // If the channel is closed there's no output to read
    if (ssh_channel_is_closed(channel.get()))
    {
        mpl::log(mpl::Level::debug, category,
                 fmt::format("{}:{} {}(): channel closed", __FILE__, __LINE__, __FUNCTION__));
        return std::string();
    }

    std::stringstream output;
    std::array<char, 256> buffer;
    int num_bytes{0};
    const bool is_std_err = type == StreamType::err;
    do
    {
        num_bytes = ssh_channel_read_timeout(channel.get(), buffer.data(), buffer.size(), is_std_err, timeout);
        mpl::log(mpl::Level::debug, category,
                 fmt::format("{}:{} {}(): num_bytes = {}", __FILE__, __LINE__, __FUNCTION__, num_bytes));
        if (num_bytes < 0)
        {
            // Latest libssh now returns an error if the channel has been closed instead of returning 0 bytes
            if (ssh_channel_is_closed(channel.get()))
            {
                mpl::log(mpl::Level::debug, category,
                         fmt::format("{}:{} {}(): channel closed", __FILE__, __LINE__, __FUNCTION__));
                return output.str();
            }

            throw mp::SSHException(
                fmt::format("error while reading ssh channel for remote process '{}' - error: {}", cmd, num_bytes));
        }
        output.write(buffer.data(), num_bytes);
    } while (num_bytes > 0);

    return output.str();
}

ssh_channel mp::SSHProcess::release_channel()
{
    return channel.release();
}

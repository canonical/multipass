/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include <multipass/ssh/ssh_process.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/ssh/throw_on_error.h>

#include <fmt/format.h>
#include <libssh/callbacks.h>

#include <array>
#include <sstream>

namespace mp = multipass;

namespace
{
class ExitStatusCallback
{
public:
    ExitStatusCallback(ssh_channel channel, mp::optional<int>& exit_status) : channel{channel}
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
        auto exit_code = reinterpret_cast<mp::optional<int>*>(userdata);
        *exit_code = exit_status;
    }
    ssh_channel channel;
    ssh_channel_callbacks_struct cb{};
};

auto make_channel(ssh_session ssh_session, const std::string& cmd)
{
    if (!ssh_is_connected(ssh_session))
        throw std::runtime_error("SSH session is not connected");

    mp::SSHProcess::ChannelUPtr channel{ssh_channel_new(ssh_session), ssh_channel_free};
    mp::SSH::throw_on_error(ssh_channel_open_session, channel);
    mp::SSH::throw_on_error(ssh_channel_request_exec, channel, cmd.c_str());
    return channel;
}
}

mp::SSHProcess::SSHProcess(ssh_session session, const std::string& cmd)
    : session{session}, cmd{cmd}, channel{make_channel(session, cmd)}
{
}

int mp::SSHProcess::exit_code(std::chrono::milliseconds timeout)
{
    ExitStatusCallback cb{channel.get(), exit_status};

    std::unique_ptr<ssh_event_struct, decltype(ssh_event_free)*> event{ssh_event_new(), ssh_event_free};
    ssh_event_add_session(event.get(), session);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    int rc{SSH_OK};
    while ((std::chrono::steady_clock::now() < deadline) && rc == SSH_OK && !exit_status)
    {
        rc = ssh_event_dopoll(event.get(), timeout.count());
    }

    if (!exit_status)
        throw std::runtime_error(fmt::format("failed to obtain exit status for remote process: \'{}\'", cmd));

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
    std::stringstream output;
    std::array<char, 256> buffer;
    int num_bytes{0};
    const bool is_std_err = type == StreamType::err ? true : false;
    do
    {
        num_bytes = ssh_channel_read_timeout(channel.get(), buffer.data(), buffer.size(), is_std_err, timeout);
        if (num_bytes < 0)
            throw std::runtime_error("error reading ssh channel");
        output.write(buffer.data(), num_bytes);
    } while (num_bytes > 0);

    return output.str();
}

ssh_channel mp::SSHProcess::release_channel()
{
    return channel.release();
}

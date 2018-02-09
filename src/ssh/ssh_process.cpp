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

#include <array>
#include <sstream>

namespace mp = multipass;

namespace
{
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

mp::SSHProcess::SSHProcess(ssh_session session, const std::string& cmd) : channel{make_channel(session, cmd)}
{
}

int mp::SSHProcess::exit_code()
{
    // Warning: This may block indefinitely if the underlying command is still running
    return ssh_channel_get_exit_status(channel.get());
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

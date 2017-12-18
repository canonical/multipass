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

mp::SSHProcess::SSHProcess(ssh_session ssh_session) : channel{ssh_channel_new(ssh_session), ssh_channel_free}
{
    SSH::throw_on_error(ssh_channel_open_session, channel);
}

void mp::SSHProcess::exec(const std::string& cmd)
{
    SSH::throw_on_error(ssh_channel_request_exec, channel, cmd.c_str());
}

int mp::SSHProcess::exit_code()
{
    // Warning: This may block indefinitely if the underlying command is still running
    return ssh_channel_get_exit_status(channel.get());
}

std::vector<std::string> mp::SSHProcess::get_output_streams()
{
    return {read_stream(StreamType::out), read_stream(StreamType::err)};
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
        output.write(buffer.data(), num_bytes);
    } while (num_bytes > 0);

    if (num_bytes < 0)
        throw std::runtime_error("error reading ssh channel");

    return output.str();
}

ssh_channel mp::SSHProcess::release_channel()
{
    return channel.release();
}

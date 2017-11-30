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

#ifndef MULTIPASS_SSH_PROCESS_H
#define MULTIPASS_SSH_PROCESS_H

#include <libssh/libssh.h>

#include <memory>
#include <string>
#include <vector>

namespace multipass
{
class SSHProcess
{
public:
    using ChannelUPtr = std::unique_ptr<ssh_channel_struct, void (*)(ssh_channel)>;

    SSHProcess(ssh_session ssh_session);

    void exec(const std::string& cmd);
    int exit_code();
    std::vector<std::string> get_output_streams();

private:
    enum class StreamType
    {
        out,
        err
    };

    std::string read_stream(StreamType type, int timeout = -1);
    ssh_channel get_ssh_channel_ptr();

    ChannelUPtr channel;

    friend class SshfsMount;
};
}
#endif // MULTIPASS_SSH_PROCESS_H

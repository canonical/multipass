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

#pragma once

#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/ssh/ssh_client.h>
#include <multipass/ssh/ssh_coordinates.h>

#include <memory>
#include <string>
#include <vector>

struct ssh_channel_struct;
struct ssh_session_struct;

namespace multipass
{

using SSHSessionUPtr = std::unique_ptr<SSHSession>;
using ssh_channel = ssh_channel_struct*;
using ssh_session = ssh_session_struct*;

// TODO@sshclient: document PlainSSHClient (single-instance guard, exec/shell flow)
class PlainSSHClient : public SSHClient, public PrivatePassProvider<PlainSSHClient>
{
public:
    using ChannelUPtr = std::unique_ptr<ssh_channel_struct, void (*)(ssh_channel)>;

    PlainSSHClient(PlainSSHSession&& ssh_session_obj);
    ~PlainSSHClient() override;

    VMReturnCode exec(const std::vector<std::vector<std::string>>& args_list) override;
    VMReturnCode open_shell() override;

private:
    VMReturnCode exec_string(const std::string& cmd_line);
    VMReturnCode get_ssh_exit_code();

    PlainSSHSession plain_ssh_session;

protected:
    ChannelUPtr channel;

    // TODO@sshclient: document handle_ssh_events contract
    virtual void handle_ssh_events(ssh_session session) = 0;
};
} // namespace multipass

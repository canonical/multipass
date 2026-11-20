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

#include <multipass/logging/log.h>
#include <multipass/rpc/multipass.pb.h>
#include <multipass/ssh/libssh_wrapper.h>
#include <multipass/ssh/plain_ssh_client.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/ssh/platform_ssh_client.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include <atomic>
#include <functional>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{

// Due to static variable needs, only one SSHClient instance can exist at a time
std::atomic<bool> active_ssh_client{false};

mp::PlainSSHClient::ChannelUPtr make_channel(ssh_session session)
{
    mp::PlainSSHClient::ChannelUPtr channel{MP_LIBSSH.ssh_channel_new(session),
                                            [](ssh_channel ch) { MP_LIBSSH.ssh_channel_free(ch); }};

    mp::SSH::throw_on_error(
        channel,
        session,
        "[ssh client] channel creation failed",
        std::bind_front(&mp::Libssh::ssh_channel_open_session, &mp::Libssh::instance()));

    return channel;
}
} // namespace

mp::PlainSSHClient::PlainSSHClient(PlainSSHSession&& ssh_session_obj)
    : plain_ssh_session{std::move(ssh_session_obj)},
      channel{make_channel(this->plain_ssh_session.borrow_session(pass))}
{
    if (active_ssh_client.exchange(true))
    {
        throw std::logic_error{"There can only be one SSHClient instance"};
    }
}

mp::PlainSSHClient::~PlainSSHClient()
{
    active_ssh_client = false;
}

mp::VMReturnCode mp::PlainSSHClient::open_shell()
{
    return exec(std::vector<std::vector<std::string>>{});
}

mp::VMReturnCode mp::PlainSSHClient::exec(const std::vector<std::vector<std::string>>& args_list)
{
    // TODO@sshclient: Improve arg parsing
    std::string cmd_line;

    if (args_list.size())
    {
        auto args_it = args_list.begin();
        cmd_line = utils::to_cmd(*args_it++, mp::utils::QuoteType::quote_every_arg);
        for (; args_it != args_list.end(); ++args_it)
            cmd_line += "&&" + utils::to_cmd(*args_it, mp::utils::QuoteType::quote_every_arg);
    }

    return exec_string(cmd_line);
}

mp::VMReturnCode mp::PlainSSHClient::exec_string(const std::string& cmd_line)
{
    // All returned ints from this function can be considered to be VMReturnCodes
    if (cmd_line.empty())
        SSH::throw_on_error(
            channel,
            plain_ssh_session.borrow_session(pass),
            "[ssh client] shell request failed",
            std::bind_front(&Libssh::ssh_channel_request_shell, &Libssh::instance()));
    else
        SSH::throw_on_error(channel,
                            plain_ssh_session.borrow_session(pass),
                            "[ssh client] exec request failed",
                            std::bind_front(&Libssh::ssh_channel_request_exec, &Libssh::instance()),
                            cmd_line.c_str());

    handle_ssh_events(plain_ssh_session.borrow_session(pass));
    return this->get_ssh_exit_code();
}

mp::VMReturnCode mp::PlainSSHClient::get_ssh_exit_code()
{

    uint32_t exit_status = static_cast<uint32_t>(-1);
    char* exit_signal_status = nullptr;
    int core_dumped = 0;

    SSH::throw_on_error(channel,
                        plain_ssh_session.borrow_session(pass),
                        "[ssh client] could not obtain exit state",
                        std::bind_front(&Libssh::ssh_channel_get_exit_state, &Libssh::instance()),
                        &exit_status,
                        &exit_signal_status,
                        &core_dumped);

    if (exit_signal_status != nullptr)
    {
        mpl::error("ssh client",
                   "Process terminated by signal: {}{}",
                   exit_signal_status,
                   core_dumped ? " (core dumped)" : "");

        MP_LIBSSH.ssh_string_free_char(exit_signal_status);
    }
    return static_cast<mp::VMReturnCode>(exit_status);
}

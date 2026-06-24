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
#include <multipass/ssh/libssh.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/ssh/ssh_client.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include "ssh_client_key_provider.h"

#ifdef MULTIPASS_PLATFORM_WINDOWS
#include <io.h>
#include <thread>
#endif

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
mp::SSHClient::ChannelUPtr make_channel(ssh_session session)
{
    mp::SSHClient::ChannelUPtr channel{MP_LIBSSH.ssh_channel_new(session), ssh_channel_free};

    mp::SSH::throw_on_error(channel,
                            session,
                            "[ssh client] channel creation failed",
                            [](ssh_channel ch) { return MP_LIBSSH.ssh_channel_open_session(ch); });

    return channel;
}

std::unique_ptr<mp::PlainSSHSession> make_session(const std::string& host,
                                                  int port,
                                                  const std::string& username,
                                                  const std::string& priv_key_blob)
{
    return std::make_unique<mp::PlainSSHSession>(host,
                                                 port,
                                                 username,
                                                 mp::SSHClientKeyProvider(priv_key_blob));
}
} // namespace

mp::SSHClient::SSHClient(const std::string& host,
                         int port,
                         const std::string& username,
                         const std::string& priv_key_blob,
                         ConsoleCreator console_creator)
    : SSHClient{make_session(host, port, username, priv_key_blob), console_creator}
{
}

mp::SSHClient::SSHClient(SSHSessionUPtr ssh_session, ConsoleCreator console_creator)
    : ssh_session{std::move(ssh_session)},
      channel{make_channel(*this->ssh_session)},
      console{console_creator(channel.get())}
{
}

int mp::SSHClient::connect()
{
    return exec(std::vector<std::vector<std::string>>{});
}

int mp::SSHClient::exec(const std::vector<std::vector<std::string>>& args_list)
{
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

void mp::SSHClient::handle_ssh_events()
{
#ifndef MULTIPASS_PLATFORM_WINDOWS
    using ConnectorUPtr = std::unique_ptr<ssh_connector_struct, void (*)(ssh_connector)>;
    std::unique_ptr<ssh_event_struct, void (*)(ssh_event)> event{MP_LIBSSH.ssh_event_new(),
                                                                 ssh_event_free};

    // stdin
    ConnectorUPtr connector_in{MP_LIBSSH.ssh_connector_new(*ssh_session), ssh_connector_free};
    MP_LIBSSH.ssh_connector_set_out_channel(connector_in.get(),
                                            channel.get(),
                                            SSH_CONNECTOR_STDOUT);
    MP_LIBSSH.ssh_connector_set_in_fd(connector_in.get(), fileno(stdin));
    MP_LIBSSH.ssh_event_add_connector(event.get(), connector_in.get());

    // stdout
    ConnectorUPtr connector_out{MP_LIBSSH.ssh_connector_new(*ssh_session), ssh_connector_free};
    MP_LIBSSH.ssh_connector_set_out_fd(connector_out.get(), fileno(stdout));
    MP_LIBSSH.ssh_connector_set_in_channel(connector_out.get(),
                                           channel.get(),
                                           SSH_CONNECTOR_STDOUT);
    MP_LIBSSH.ssh_event_add_connector(event.get(), connector_out.get());

    // stderr
    ConnectorUPtr connector_err{MP_LIBSSH.ssh_connector_new(*ssh_session), ssh_connector_free};
    MP_LIBSSH.ssh_connector_set_out_fd(connector_err.get(), fileno(stderr));
    MP_LIBSSH.ssh_connector_set_in_channel(connector_err.get(),
                                           channel.get(),
                                           SSH_CONNECTOR_STDERR);
    MP_LIBSSH.ssh_event_add_connector(event.get(), connector_err.get());

    while (MP_LIBSSH.ssh_channel_is_open(channel.get()) &&
           !MP_LIBSSH.ssh_channel_is_eof(channel.get()))
    {
        MP_LIBSSH.ssh_event_dopoll(event.get(), 60000);
        console->handle_runtime_events();
    }

    MP_LIBSSH.ssh_event_remove_connector(event.get(), connector_in.get());
    MP_LIBSSH.ssh_event_remove_connector(event.get(), connector_out.get());
    MP_LIBSSH.ssh_event_remove_connector(event.get(), connector_err.get());
#else
    auto stdout_thread = std::thread([this]() {
        while (MP_LIBSSH.ssh_channel_is_open(channel.get()) &&
               !MP_LIBSSH.ssh_channel_is_eof(channel.get()))
        {
            console->write_console();
        }
        console->exit_console();
    });

    while (MP_LIBSSH.ssh_channel_is_open(channel.get()) &&
           !MP_LIBSSH.ssh_channel_is_eof(channel.get()))
    {
        console->read_console();
    }

    stdout_thread.join();
#endif
}

int mp::SSHClient::exec_string(const std::string& cmd_line)
{
    // All returned ints from this function can be considered to be VMReturnCodes
    if (cmd_line.empty())
        SSH::throw_on_error(channel,
                            *ssh_session,
                            "[ssh client] shell request failed",
                            [](ssh_channel ch) { return MP_LIBSSH.ssh_channel_request_shell(ch); });
    else
        SSH::throw_on_error(
            channel,
            *ssh_session,
            "[ssh client] exec request failed",
            [](ssh_channel ch, const char* c) { return MP_LIBSSH.ssh_channel_request_exec(ch, c); },
            cmd_line.c_str());

    handle_ssh_events();
    return this->get_ssh_exit_code();
}

int mp::SSHClient::get_ssh_exit_code()
{

    uint32_t exit_status = static_cast<uint32_t>(-1);
    char* exit_signal_status = nullptr;
    int core_dumped = 0;

    SSH::throw_on_error(
        channel,
        *ssh_session,
        "[ssh client] could not obtain exit state",
        [](ssh_channel ch, uint32_t* es, char** signal, int* core) {
            return MP_LIBSSH.ssh_channel_get_exit_state(ch, es, signal, core);
        },
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
    return static_cast<int>(exit_status);
}

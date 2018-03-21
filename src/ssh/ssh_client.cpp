/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <multipass/ssh/ssh_client.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include "ssh_client_key_provider.h"

#include <libssh/libssh.h>

namespace mp = multipass;

mp::SSHClient::SSHClient(const std::string& host, int port, const std::string& priv_key_blob)
    : SSHClient{std::make_unique<mp::SSHSession>(host, port, mp::SSHClientKeyProvider(priv_key_blob))}
{
}

mp::SSHClient::SSHClient(SSHSessionUPtr ssh_session, Console::UPtr console)
    : ssh_session{std::move(ssh_session)},
      channel{ssh_channel_new(*this->ssh_session), ssh_channel_free},
      console{(console == nullptr) ? Console::make_console() : std::move(console)}
{
}

void mp::SSHClient::connect()
{
    exec({});
}

int mp::SSHClient::exec(const std::vector<std::string>& args)
{
    SSH::throw_on_error(ssh_channel_open_session, channel);

    if (console->is_interactive())
    {
        SSH::throw_on_error(ssh_channel_request_pty, channel);
        change_ssh_pty_size(console->get_window_geometry());
    }

    if (args.empty())
        SSH::throw_on_error(ssh_channel_request_shell, channel);
    else
        SSH::throw_on_error(ssh_channel_request_exec, channel,
                            utils::to_cmd(args, mp::utils::QuoteType::quote_every_arg).c_str());

    handle_ssh_events();

    return ssh_channel_get_exit_status(channel.get());
}

void mp::SSHClient::handle_ssh_events()
{
    using ConnectorUPtr = std::unique_ptr<ssh_connector_struct, void (*)(ssh_connector)>;
    std::unique_ptr<ssh_event_struct, void (*)(ssh_event)> event{ssh_event_new(), ssh_event_free};

    // stdin
    ConnectorUPtr connector_in{ssh_connector_new(*ssh_session), ssh_connector_free};
    ssh_connector_set_out_channel(connector_in.get(), channel.get(), SSH_CONNECTOR_STDOUT);
    ssh_connector_set_in_fd(connector_in.get(), fileno(stdin));
    ssh_event_add_connector(event.get(), connector_in.get());

    // stdout
    ConnectorUPtr connector_out{ssh_connector_new(*ssh_session), ssh_connector_free};
    ssh_connector_set_out_fd(connector_out.get(), fileno(stdout));
    ssh_connector_set_in_channel(connector_out.get(), channel.get(), SSH_CONNECTOR_STDOUT);
    ssh_event_add_connector(event.get(), connector_out.get());

    // stderr
    ConnectorUPtr connector_err{ssh_connector_new(*ssh_session), ssh_connector_free};
    ssh_connector_set_out_fd(connector_err.get(), fileno(stderr));
    ssh_connector_set_in_channel(connector_err.get(), channel.get(), SSH_CONNECTOR_STDERR);
    ssh_event_add_connector(event.get(), connector_err.get());

    while (ssh_channel_is_open(channel.get()) && !ssh_channel_is_eof(channel.get()))
    {
        if (console->is_window_size_changed())
            change_ssh_pty_size(console->get_window_geometry());

        ssh_event_dopoll(event.get(), 60000);
    }

    ssh_event_remove_connector(event.get(), connector_in.get());
    ssh_event_remove_connector(event.get(), connector_out.get());
    ssh_event_remove_connector(event.get(), connector_err.get());
}

void mp::SSHClient::change_ssh_pty_size(mp::Console::WindowGeometry window_geometry)
{
    SSH::throw_on_error(ssh_channel_change_pty_size, channel, window_geometry.columns, window_geometry.rows);
}

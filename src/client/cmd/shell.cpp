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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "shell.h"

#include <multipass/cli/argparser.h>
#include <multipass/ssh/ssh_client.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Shell::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::SSHInfoReply& reply) {
        // TODO: mainly for testing - need a better way to test parsing
        if (reply.ssh_info().empty())
            return ReturnCode::Ok;

        // TODO: this should setup a reader that continously prints out
        // streaming replies from the server corresponding to stdout/stderr streams
        auto ssh_info = reply.ssh_info().begin()->second;
        auto host = ssh_info.host();
        auto port = ssh_info.port();
        auto username = ssh_info.username();
        auto priv_key_blob = ssh_info.priv_key_base64();

        try
        {
            mp::SSHClient ssh_client{host, port, username, priv_key_blob};
            ssh_client.connect();
        }
        catch (const std::exception& e)
        {
            cerr << "shell failed: " << e.what() << "\n";
            return ReturnCode::CommandFail;
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "shell failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::ssh_info, request, on_success, on_failure);
}

std::string cmd::Shell::name() const { return "shell"; }

std::vector<std::string> cmd::Shell::aliases() const
{
    return {name(), "sh", "connect"};
}

QString cmd::Shell::short_help() const
{
    return QStringLiteral("Open a shell on a running instance");
}

QString cmd::Shell::description() const
{
    return QStringLiteral("Open a shell prompt on the instance.");
}

mp::ParseCode cmd::Shell::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Name of the instance to open a shell on", "<name>");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() == 0)
    {
        cerr << "Name argument is required\n";
        status = ParseCode::CommandLineError;
    }
    else if (parser->positionalArguments().count() > 1)
    {
        cerr << "Too many arguments given\n";
        status = ParseCode::CommandLineError;
    }
    else
    {
        auto entry = request.add_instance_name();
        entry->append(parser->positionalArguments().first().toStdString());
    }

    return status;
}

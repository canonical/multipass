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

#include "shell.h"
#include "common_cli.h"

#include "animated_spinner.h"
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

    // We can assume the first array entry since `shell` only uses one instance
    // at a time
    auto instance_name = request.instance_name()[0];

    auto on_success = [this](mp::SSHInfoReply& reply) {
        // TODO: mainly for testing - need a better way to test parsing
        if (reply.ssh_info().empty())
            return ReturnCode::Ok;

        // TODO: this should setup a reader that continously prints out
        // streaming replies from the server corresponding to stdout/stderr streams
        const auto& ssh_info = reply.ssh_info().begin()->second;
        const auto& host = ssh_info.host();
        const auto& port = ssh_info.port();
        const auto& username = ssh_info.username();
        const auto& priv_key_blob = ssh_info.priv_key_base64();

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

    auto on_failure = [this, &instance_name](grpc::Status& status) {
        if (status.error_code() == grpc::StatusCode::ABORTED)
        {
            return start_instance_for(instance_name);
        }
        else
        {
            return standard_failure_handler_for(name(), cerr, status);
        }
    };

    request.set_verbosity_level(parser->verbosityLevel());
    ReturnCode return_code;
    while ((return_code = dispatch(&RpcMethod::ssh_info, request, on_success, on_failure)) == ReturnCode::Retry)
        ;

    return return_code;
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

mp::ReturnCode cmd::Shell::start_instance_for(const std::string& instance_name)
{
    AnimatedSpinner spinner{cout};
    auto on_success = [this, &spinner](mp::StartReply& reply) {
        spinner.stop();
        cout << '\r' << std::flush;
        return ReturnCode::Retry;
    };

    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    StartRequest request;
    auto names = request.mutable_instance_names()->add_instance_name();
    names->append(instance_name);

    spinner.start(fmt::format("Starting {}", instance_name));
    return dispatch(&RpcMethod::start, request, on_success, on_failure);
}

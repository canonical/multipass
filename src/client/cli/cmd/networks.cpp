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

#include "networks.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Networks::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](NetworksReply& reply) {
        cout << chosen_formatter->format(reply);

        if (term->is_live() && update_available(reply.update_info()))
            cout << update_notice(reply.update_info());

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    NetworksRequest request;
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::networks, request, on_success, on_failure);
}

std::string cmd::Networks::name() const
{
    return "networks";
}

std::vector<std::string> cmd::Networks::aliases() const
{
    return {name()};
}

QString cmd::Networks::short_help() const
{
    return QStringLiteral("List available network interfaces");
}

QString cmd::Networks::description() const
{
    return QStringLiteral("List host network devices (physical interfaces, virtual switches, bridges)\n"
                          "available to integrate with using the `--network` switch to the `launch`\ncommand.");
}

mp::ParseCode cmd::Networks::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption(
        "format", "Output list in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");

    parser->addOption(formatOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no arguments\n";
        return ParseCode::CommandLineError;
    }

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}

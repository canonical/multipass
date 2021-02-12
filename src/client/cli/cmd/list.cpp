/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include "list.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::List::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](ListReply& reply) {
        cout << chosen_formatter->format(reply);

        if (term->is_live() && update_available(reply.update_info()))
            cout << update_notice(reply.update_info());

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::list, request, on_success, on_failure);
}

std::string cmd::List::name() const
{
    return "list";
}

std::vector<std::string> cmd::List::aliases() const
{
    return {name(), "ls"};
}

QString cmd::List::short_help() const
{
    return QStringLiteral("List all available instances");
}

QString cmd::List::description() const
{
    return QStringLiteral("List all instances which have been created.");
}

mp::ParseCode cmd::List::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption(
        "format", "Output list in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");

    QCommandLineOption noIpv4Option("no-ipv4", "Do not query the instances for the IPv4's they are using");
    noIpv4Option.setFlags(QCommandLineOption::HiddenFromHelp);

    parser->addOptions({formatOption, noIpv4Option});

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

    request.set_request_ipv4(!parser->isSet(noIpv4Option));

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}

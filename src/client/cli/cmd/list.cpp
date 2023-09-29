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

#include "list.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

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
    return QStringLiteral("List all available instances or snapshots");
}

QString cmd::List::description() const
{
    return QStringLiteral("List all instances or snapshots which have been created.");
}

mp::ParseCode cmd::List::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption snapshotsOption("snapshots", "List all available snapshots");
    QCommandLineOption formatOption(
        "format", "Output list in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");
    QCommandLineOption noIpv4Option("no-ipv4", "Do not query the instances for the IPv4's they are using");
    noIpv4Option.setFlags(QCommandLineOption::HiddenFromHelp);

    parser->addOptions({snapshotsOption, formatOption, noIpv4Option});

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

    if (parser->isSet(snapshotsOption) && parser->isSet(noIpv4Option))
    {
        cerr << "IP addresses are not applicable in conjunction with listing snapshots\n";
        return ParseCode::CommandLineError;
    }

    request.set_snapshots(parser->isSet(snapshotsOption));
    request.set_request_ipv4(!parser->isSet(noIpv4Option));

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}

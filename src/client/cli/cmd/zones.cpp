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

#include "zones.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Zones::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](ZonesReply& reply) {
        cout << chosen_formatter->format(reply);
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::zones, request, on_success, on_failure);
}

std::string cmd::Zones::name() const
{
    return "zones";
}

std::vector<std::string> cmd::Zones::aliases() const
{
    return {name()};
}

QString cmd::Zones::short_help() const
{
    return QStringLiteral("List all availability zones");
}

QString cmd::Zones::description() const
{
    return QStringLiteral("List all availability zones, along with their availability status.");
}

mp::ParseCode cmd::Zones::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption(
        "format",
        "Output list in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format",
        "table");

    parser->addOptions({formatOption});

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

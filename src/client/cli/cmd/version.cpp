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

#include "version.h"
#include "common_cli.h"
#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>
#include <multipass/version.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Version::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::VersionReply& reply)
    {
        cout << chosen_formatter->format(reply, multipass::version_string);

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status)
    {
        VersionReply reply;
        cout << chosen_formatter->format(reply, multipass::version_string);

        return ReturnCode::Ok;
    };

    mp::VersionRequest request;
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::version, request, on_success, on_failure);
}

std::string cmd::Version::name() const
{
    return "version";
}

QString cmd::Version::short_help() const
{
    return QStringLiteral("Show version details");
}

QString cmd::Version::description() const
{
    return QStringLiteral("Display version information about the multipass command\n"
                          "and daemon.");
}

mp::ParseCode cmd::Version::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption("format",
                                    "Output version information in the requested format.\n"
                                    "Valid formats are: table (default), json, csv and yaml",
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

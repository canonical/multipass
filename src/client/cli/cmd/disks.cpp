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

#include "disks.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>
#include <multipass/logging/log.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Disks::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::ListDisksReply& reply) {
        multipass::logging::log(multipass::logging::Level::info,
                                "disks_cli",
                                "Successfully received disks list reply");
        cout << chosen_formatter->format(reply);
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        multipass::logging::log(multipass::logging::Level::error,
                                "disks_cli",
                                fmt::format("Failed to list disks: {}", status.error_message()));
        return standard_failure_handler_for(name(), cerr, status);
    };

    request.set_verbosity_level(parser->verbosityLevel());
    multipass::logging::log(multipass::logging::Level::info,
                            "disks_cli",
                            "Sending list disks request to daemon");
    return dispatch(&RpcMethod::list_disks, request, on_success, on_failure);
}

std::string cmd::Disks::name() const
{
    return "disks";
}

std::vector<std::string> cmd::Disks::aliases() const
{
    return {name()};
}

QString cmd::Disks::short_help() const
{
    return QStringLiteral("List available extra disks");
}

QString cmd::Disks::description() const
{
    return QStringLiteral("List all available extra disks that can be attached to instances.");
}

mp::ParseCode cmd::Disks::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption("format",
                                    "Output list in the requested format.\n"
                                    "Valid formats are: table (default), json, csv and yaml",
                                    "format",
                                    "table");

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

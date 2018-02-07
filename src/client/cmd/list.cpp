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

#include "list.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/json_output.h>
#include <multipass/cli/table_output.h>

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
        std::string output;

        if (format_type == "json")
        {
            JsonOutput json_output;
            output = json_output.process_list(reply);
        }
        else
        {
            const auto size = reply.instances_size();
            if (size < 1)
            {
                output = "No instances found.\n";
            }
            else
            {
                TableOutput table_output;

                output = table_output.process_list(reply);
            }
        }

        cout << output;
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "list failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    ListRequest request;
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
    QCommandLineOption formatOption("format",
                                    "Output list in the requested format.\nValid formats are: table (default) and json",
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

    if (parser->isSet(formatOption))
    {
        QString format_value{parser->value(formatOption)};
        QStringList valid_formats{"table", "json"};

        if (!valid_formats.contains(format_value))
        {
            cerr << "Invalid format type given.\n";
            status = ParseCode::CommandLineError;
        }
        else
            format_type = format_value;
    }

    return status;
}

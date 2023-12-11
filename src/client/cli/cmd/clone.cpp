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

#include "clone.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Clone::run(ArgParser* parser)
{
    const auto parscode = parse_args(parser);
    if (parscode != ParseCode::Ok)
    {
        return parser->returnCodeFrom(parscode);
    }

    auto action_on_success = [this](CloneReply& reply) -> ReturnCode {
        cout << reply.reply_message();

        return ReturnCode::Ok;
    };

    auto action_on_failure = [this](grpc::Status& status, CloneReply& reply) -> ReturnCode {
        return standard_failure_handler_for(name(), cerr, status, reply.reply_message());
    };

    rpc_request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::clone, rpc_request, action_on_success, action_on_failure);
}

std::string cmd::Clone::name() const
{
    return "clone";
}

QString cmd::Clone::short_help() const
{
    return QStringLiteral("Clone a Ubuntu instance");
}

QString cmd::Clone::description() const
{
    return QStringLiteral("A clone is a complete independent copy of a whole virtual machine instance");
}

mp::ParseCode cmd::Clone::parse_args(ArgParser* parser)
{
    parser->addPositionalArgument("source_name", "The name of the source virtual machine instance", "<source_name>");

    const QCommandLineOption destination_name_option{
        {"n", "name"},
        "An optional name for the destination instance name, it obeys the same validity rules as instance  names (see "
        "help launch`). Default: \"<source_name>-cloneN\", where N is the Nth cloned instance of the original instance",
        "destination name"};

    parser->addOption(destination_name_option);

    const auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    const auto number_of_positional_arguments = parser->positionalArguments().count();
    if (number_of_positional_arguments < 1)
    {
        cerr << "Please provide the name of the source instance.\n";
        return ParseCode::CommandLineError;
    }

    if (number_of_positional_arguments > 1)
    {
        cerr << "Too many arguments.\n";
        return ParseCode::CommandLineError;
    }

    const auto& source_name = parser->positionalArguments()[0];
    rpc_request.set_source_name(source_name.toStdString());
    if (parser->isSet(destination_name_option))
    {
        rpc_request.set_destination_name(parser->value(destination_name_option).toStdString());
    }

    return ParseCode::Ok;
}

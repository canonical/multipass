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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *
 */

#include "info.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Info::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::InfoReply& reply) {
        cout << chosen_formatter->format(reply);

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "info failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::info, request, on_success, on_failure);
}

std::string cmd::Info::name() const { return "info"; }

QString cmd::Info::short_help() const
{
    return QStringLiteral("Display information about instances");
}

QString cmd::Info::description() const
{
    return QStringLiteral("Display information about instances");
}

mp::ParseCode cmd::Info::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to display information about", "<name> [<name> ...]");

    QCommandLineOption all_option("all", "Display info for all instances");
    parser->addOption(all_option);

    QCommandLineOption formatOption(
        "format", "Output info in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");
    parser->addOption(formatOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    auto num_names = parser->positionalArguments().count();
    if (num_names == 0 && !parser->isSet(all_option))
    {
        cerr << "Name argument or --all is required\n";
        return ParseCode::CommandLineError;
    }

    if (num_names > 0 && parser->isSet(all_option))
    {
        cerr << "Cannot specify name";
        if (num_names > 1)
            cerr << "s";
        cerr << " when --all option set\n";
        return ParseCode::CommandLineError;
    }

    for (const auto& arg : parser->positionalArguments())
    {
        auto entry = request.add_instance_name();
        entry->append(arg.toStdString());
    }

    QString format_value{parser->value(formatOption)};
    chosen_formatter = formatter_for(format_value.toStdString());

    if (chosen_formatter == nullptr)
    {
        cerr << "Invalid format type given.\n";
        status = ParseCode::CommandLineError;
    }

    return status;
}

/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

#include "info.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/formatter.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

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

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
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

    QCommandLineOption all_option(all_option_name, "Display info for all instances");
    parser->addOption(all_option);

    QCommandLineOption noRuntimeInfoOption(
        "no-runtime-information",
        "Retrieve from the daemon only the information obtained without running commands on the instance");
    noRuntimeInfoOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser->addOption(noRuntimeInfoOption);

    QCommandLineOption formatOption(
        "format", "Output info in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");
    parser->addOption(formatOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr);
    if (parse_code != ParseCode::Ok)
        return parse_code;

    request.mutable_instance_names()->CopyFrom(add_instance_names(parser));
    request.set_no_runtime_information(parser->isSet(noRuntimeInfoOption));

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}

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

        if (term->is_live() && update_available(reply.update_info()))
            cout << update_notice(reply.update_info());

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::info, request, on_success, on_failure);
}

std::string cmd::Info::name() const { return "info"; }

QString cmd::Info::short_help() const
{
    return QStringLiteral("Display information about instances or snapshots");
}

QString cmd::Info::description() const
{
    return short_help();
}

mp::ParseCode cmd::Info::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("instance/snapshot",
                                  "Names of instances or snapshots to display information about",
                                  "<instance>[.snapshot] [<instance>[.snapshot] ...]");

    QCommandLineOption all_option(all_option_name, "Display info for all instances.");
    all_option.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption noRuntimeInfoOption(
        "no-runtime-information",
        "Retrieve from the daemon only the information obtained without running commands on the instance.");
    noRuntimeInfoOption.setFlags(QCommandLineOption::HiddenFromHelp);
    QCommandLineOption snapshots_option{"snapshots",
                                        "Display detailed information about the snapshots of specified instances. This "
                                        "option has no effect on snapshot arguments. Omit instance/snapshot arguments "
                                        "to obtain detailed information on all the snapshots of all instances."};
    QCommandLineOption format_option(
        format_option_name,
        "Output info in the requested format.\nValid formats are: table (default), json, csv and yaml.",
        format_option_name,
        "table");

    parser->addOptions({all_option, noRuntimeInfoOption, snapshots_option, format_option});

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    status = handle_format_option(parser, &chosen_formatter, cerr);

    status = check_for_name_and_all_option_conflict(parser, cerr, true);
    if (status != ParseCode::Ok)
        return status;

    if (parser->isSet(all_option_name))
        cerr << "Warning: the `--all` flag for the `info` command is deprecated. Please use `info` with no positional "
                "arguments for the same effect.\n";

    bool instance_found = false, snapshot_found = false;
    for (const auto& item : add_instance_and_snapshot_names(parser))
    {
        if (!item.has_snapshot_name())
            instance_found = true;
        else
            snapshot_found = true;

        request.add_instance_snapshot_pairs()->CopyFrom(item);
    }

    request.set_no_runtime_information(parser->isSet(noRuntimeInfoOption));

    const auto& snapshots_only = parser->isSet(snapshots_option);
    request.set_snapshots(snapshots_only);

    if (instance_found && snapshot_found && parser->value(format_option_name) == "csv" && !snapshots_only)
    {
        cerr << "Mixed snapshot and instance arguments are not supported with CSV format\n";
        return ParseCode::CommandLineError;
    }

    return status;
}

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

#include "snapshot.h"

#include "animated_spinner.h"
#include "common_callbacks.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = mp::cmd;

mp::ReturnCode cmd::Snapshot::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    AnimatedSpinner spinner{cout};

    auto on_success = [this, &spinner](mp::SnapshotReply& reply) {
        spinner.stop();
        fmt::print(cout, "Snapshot taken: {}.{}\n", request.instance(), reply.snapshot());
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    spinner.start("Taking snapshot");
    return dispatch(&RpcMethod::snapshot,
                    request,
                    on_success,
                    on_failure,
                    make_logging_spinner_callback<SnapshotRequest, SnapshotReply>(spinner, cerr));
}

std::string cmd::Snapshot::name() const
{
    return "snapshot";
}

QString cmd::Snapshot::short_help() const
{
    return QStringLiteral("Take a snapshot of an instance");
}

QString cmd::Snapshot::description() const
{
    return QStringLiteral(
        "Take a snapshot of a stopped instance that can later be restored to recover the current state.");
}

mp::ParseCode cmd::Snapshot::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("instance", "The instance to take a snapshot of.");
    QCommandLineOption name_opt({"n", "name"},
                                "An optional name for the snapshot, subject to the same validity rules as instance "
                                "names (see `help launch`). Default: \"snapshotN\", where N is one plus the "
                                "number of snapshots that were ever taken for <instance>.",
                                "name");
    QCommandLineOption comment_opt{{"comment", "c", "m"},
                                   "An optional free comment to associate with the snapshot. (Hint: quote the text to "
                                   "avoid spaces being parsed by your shell)",
                                   "comment"};
    parser->addOptions({name_opt, comment_opt});

    if (auto status = parser->commandParse(this); status != ParseCode::Ok)
        return status;

    const auto positional_args = parser->positionalArguments();
    const auto num_args = positional_args.count();
    if (num_args < 1)
    {
        cerr << "Need the name of an instance to snapshot.\n";
        return ParseCode::CommandLineError;
    }

    if (num_args > 1)
    {
        cerr << "Too many arguments supplied\n";
        return ParseCode::CommandLineError;
    }

    request.set_instance(positional_args.first().toStdString());
    request.set_comment(parser->value(comment_opt).toStdString());
    request.set_snapshot(parser->value(name_opt).toStdString());
    request.set_verbosity_level(parser->verbosityLevel());

    return ParseCode::Ok;
}

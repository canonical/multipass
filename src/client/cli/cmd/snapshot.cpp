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
#include <multipass/cli/client_common.h>
#include <multipass/cli/prompters.h>

namespace mp = multipass;
namespace cmd = mp::cmd;

mp::ReturnCodeVariant cmd::Snapshot::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    AnimatedSpinner spinner{cout};

    using Client = grpc::ClientReaderWriterInterface<SnapshotRequest, SnapshotReply>;
    auto streaming_callback = [this, &spinner](const mp::SnapshotReply& reply, Client* client) {
        if (!reply.log_line().empty())
            spinner.print(cerr, reply.log_line());

        if (const auto& msg = reply.reply_message(); !msg.empty())
        {
            spinner.stop();
            spinner.start(msg);
        }

        if (reply.needs_prompt())
        {
            spinner.stop();

            if (!term->is_live())
            {
                spinner.print(
                    cerr,
                    "Unable to query client for confirmation. Use '--restart' to automatically "
                    "stop the running instance, take the snapshot and restart it.\n");
                client->WritesDone();
                return;
            }

            SnapshotRequest client_response;
            client_response.set_restart(confirm_restart(request.instance(), reply.is_suspended()));
            client->Write(client_response);
            spinner.start();
        }
    };

    auto on_success = [this, &spinner](mp::SnapshotReply& reply) -> ReturnCodeVariant {
        spinner.stop();
        fmt::print(cout, "Snapshot '{}' of '{}' created.\n", reply.snapshot(), request.instance());
        if (auto warnings = reply.warnings(); !warnings.empty())
            fmt::print(cout, "warning: {}\n", warnings);
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner](grpc::Status& status) -> ReturnCodeVariant {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    return dispatch(&RpcMethod::snapshot, request, on_success, on_failure, streaming_callback);
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
        "Take a snapshot of an instance that can later be restored to recover the current "
        "state.\nInfo: if the instance is running, you will be prompted to stop it before "
        "taking the snapshot, unless the '--restart' option is given.");
}

mp::ParseCode cmd::Snapshot::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("instance", "The instance to take a snapshot of.");
    QCommandLineOption name_opt(
        {"n", "name"},
        "An optional name for the snapshot, subject to the same validity rules as instance "
        "names (see `help launch`). Default: \"snapshotN\", where N is one plus the "
        "number of snapshots that were ever taken for <instance>.",
        "name");
    QCommandLineOption comment_opt{
        {"comment", "c", "m"},
        "An optional free comment to associate with the snapshot. (Hint: quote the text to "
        "avoid spaces being parsed by your shell)",
        "comment"};
    QCommandLineOption restart_opt{
        {"restart", "r"},
        "Stop the instance if it is running before taking the snapshot and start it after the "
        "snapshot has been taken without an interactive prompt."};
    parser->addOptions({name_opt, comment_opt, restart_opt});

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
    request.set_restart(parser->isSet(restart_opt));

    return ParseCode::Ok;
}

bool cmd::Snapshot::confirm_restart(const std::string& instance_name, bool is_suspended)
{
    const auto running_prompt_text = fmt::format(
        "Instance '{}' is running. Would you like to stop it, take a snapshot, and restart the "
        "instance?[y/N]",
        instance_name);
    const auto suspended_prompt_text = fmt::format(
        "Instance '{}' is suspended. Would you like to resume and stop it, take a snapshot, and "
        "then restart the instance?[y/N]",
        instance_name);
    static constexpr auto invalid_input = "Please answer [y/N]";
    mp::PlainPrompter prompter(term);

    auto answer = prompter.prompt(is_suspended ? suspended_prompt_text : running_prompt_text);
    while (!answer.empty() && !std::regex_match(answer, mp::client::yes_answer) &&
           !std::regex_match(answer, mp::client::no_answer))
        answer = prompter.prompt(invalid_input);

    return std::regex_match(answer, mp::client::yes_answer);
}

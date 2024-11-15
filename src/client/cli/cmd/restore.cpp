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

#include "restore.h"
#include "animated_spinner.h"
#include "common_callbacks.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/prompters.h>
#include <multipass/exceptions/cli_exceptions.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Restore::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    AnimatedSpinner spinner{cout};

    auto on_success = [this, &spinner](mp::RestoreReply& reply) {
        spinner.stop();
        fmt::print(cout, "Snapshot restored: {}.{}\n", request.instance(), request.snapshot());
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    using Client = grpc::ClientReaderWriterInterface<RestoreRequest, RestoreReply>;
    auto streaming_callback = [this, &spinner](const mp::RestoreReply& reply, Client* client) {
        if (!reply.log_line().empty())
            spinner.print(cerr, reply.log_line());

        if (const auto& msg = reply.reply_message(); !msg.empty())
        {
            spinner.stop();
            spinner.start(msg);
        }

        if (reply.confirm_destructive())
        {
            spinner.stop();
            RestoreRequest client_response;

            if (term->is_live())
                client_response.set_destructive(confirm_destruction(request.instance()));
            else
                throw std::runtime_error("Unable to query client for confirmation. Use '--destructive' to "
                                         "automatically discard current machine state.");

            client->Write(client_response);
            spinner.start();
        }
    };

    return dispatch(&RpcMethod::restore, request, on_success, on_failure, streaming_callback);
}

std::string cmd::Restore::name() const
{
    return "restore";
}

QString cmd::Restore::short_help() const
{
    return QStringLiteral("Restore an instance from a snapshot");
}

QString cmd::Restore::description() const
{
    return QStringLiteral("Restore a stopped instance to the state of a previously taken snapshot.");
}

mp::ParseCode cmd::Restore::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("instance.snapshot",
                                  "The instance to restore and snapshot to use, in <instance>.<snapshot> format, where "
                                  "<instance> is the name of an instance, and <snapshot> is the name of a snapshot",
                                  "<instance>.<snapshot>");

    QCommandLineOption destructive({"d", "destructive"}, "Discard the current state of the instance");
    parser->addOption(destructive);

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    const auto positional_args = parser->positionalArguments();
    const auto num_args = positional_args.count();
    if (num_args < 1)
    {
        cerr << "Need the name of an instance and snapshot to restore.\n";
        return ParseCode::CommandLineError;
    }

    if (num_args > 1)
    {
        cerr << "Too many arguments supplied.\n";
        return ParseCode::CommandLineError;
    }

    const auto tokens = parser->positionalArguments().at(0).split('.');
    if (tokens.size() != 2 || tokens[0].isEmpty() || tokens[1].isEmpty())
    {
        cerr << "Invalid format. Please specify the instance to restore and snapshot to use in the form "
                "<instance>.<snapshot>.\n";
        return ParseCode::CommandLineError;
    }

    request.set_instance(tokens[0].toStdString());
    request.set_snapshot(tokens[1].toStdString());
    request.set_destructive(parser->isSet(destructive));
    request.set_verbosity_level(parser->verbosityLevel());

    return ParseCode::Ok;
}

bool cmd::Restore::confirm_destruction(const std::string& instance_name)
{
    static constexpr auto prompt_text =
        "Do you want to take a snapshot of {} before discarding its current state? (Yes/no)";
    static constexpr auto invalid_input = "Please answer Yes/no";
    mp::PlainPrompter prompter(term);

    auto answer = prompter.prompt(fmt::format(prompt_text, instance_name));
    while (!answer.empty() && !std::regex_match(answer, mp::client::yes_answer) &&
           !std::regex_match(answer, mp::client::no_answer))
        answer = prompter.prompt(invalid_input);

    return std::regex_match(answer, mp::client::no_answer);
}

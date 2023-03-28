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

#include <regex>

namespace mp = multipass;
namespace cmd = mp::cmd;

namespace
{
const std::regex yes{"y|yes", std::regex::icase | std::regex::optimize};
const std::regex no{"n|no", std::regex::icase | std::regex::optimize};
} // namespace

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

    spinner.start("Restoring snapshot ");
    return dispatch(&RpcMethod::restore, request, on_success, on_failure,
                    make_logging_spinner_callback<RestoreRequest, RestoreReply>(spinner, cerr));
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
    return QStringLiteral("Restore an instance to the state of a previously taken snapshot.");
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
                "<instance>.<spanshot>.\n";
        return ParseCode::CommandLineError;
    }

    request.set_instance(tokens[0].toStdString());
    request.set_snapshot(tokens[1].toStdString());
    request.set_destructive(parser->isSet(destructive));

    if (!parser->isSet(destructive))
    {
        if (term->is_live())
        {
            try
            {
                request.set_destructive(confirm_destruction(tokens[0]));
            }
            catch (const mp::PromptException& e)
            {
                std::cerr << e.what() << std::endl;
                return ParseCode::CommandLineError;
            }
        }
        else
        {
            return ParseCode::CommandFail;
        }
    }

    request.set_verbosity_level(parser->verbosityLevel());

    return ParseCode::Ok;
}

bool cmd::Restore::confirm_destruction(const QString& instance_name)
{
    static constexpr auto prompt_text =
        "Do you want to take a snapshot of {} before discarding its current state? (Yes/no)";
    static constexpr auto invalid_input = "Please answer yes/no";
    mp::PlainPrompter prompter(term);

    auto answer = prompter.prompt(fmt::format(prompt_text, instance_name));
    while (!std::regex_match(answer, yes) && !std::regex_match(answer, no))
        answer = prompter.prompt(invalid_input);

    return std::regex_match(answer, no);
}

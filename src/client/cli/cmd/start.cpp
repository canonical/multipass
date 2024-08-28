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

#include "start.h"

#include "animated_spinner.h"
#include "common_callbacks.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/constants.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/settings/settings.h>
#include <multipass/timer.h>

#include <fmt/ostream.h>

#include <cassert>
#include <chrono>
#include <cstdlib>

namespace mp = multipass;
namespace cmd = multipass::cmd;

using namespace std::chrono_literals;

namespace
{
constexpr auto deleted_error_fmt =
    "Instance '{}' is deleted. Use 'recover' to recover it or 'purge' to permanently delete it.\n";
constexpr auto absent_error_fmt = "Instance '{}' does not exist.\n";
constexpr auto unknown_error_fmt = "Instance '{}' failed in an unexpected way, check logs for more information.\n";
} // namespace

mp::ReturnCode cmd::Start::run(mp::ArgParser* parser)
{
    petenv_name = MP_SETTINGS.get(petenv_key);
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    AnimatedSpinner spinner{cout};

    auto on_success = [&spinner, this](mp::StartReply& reply) {
        spinner.stop();
        if (term->is_live() && update_available(reply.update_info()))
            cout << update_notice(reply.update_info());
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner, parser](grpc::Status& status) {
        spinner.stop();

        std::string details;
        if (status.error_code() == grpc::StatusCode::ABORTED && !status.error_details().empty())
        {
            mp::StartError start_error;
            start_error.ParseFromString(status.error_details());

            for (const auto& pair : start_error.instance_errors())
            {
                const auto* err_fmt = unknown_error_fmt;
                if (pair.second == mp::StartError::INSTANCE_DELETED)
                    err_fmt = deleted_error_fmt;
                else if (pair.second == mp::StartError::DOES_NOT_EXIST)
                {
                    if (pair.first != petenv_name.toStdString())
                        err_fmt = absent_error_fmt;
                    else
                        continue;
                }

                fmt::format_to(std::back_inserter(details), err_fmt, pair.first);
            }

            if (details.empty())
            {
                assert(start_error.instance_errors_size() == 1 &&
                       std::cbegin(start_error.instance_errors())->first == petenv_name.toStdString());

                QStringList launch_args{"multipass", "launch", "--name", petenv_name};
                if (parser->isSet("timeout"))
                    launch_args.append({"--timeout", parser->value("timeout")});

                return run_cmd_and_retry(launch_args, parser, cout, cerr); /*
                             TODO replace with create, so that all instances are started in a single go */
            }
        }

        return standard_failure_handler_for(name(), cerr, status, details);
    };

    request.set_verbosity_level(parser->verbosityLevel());

    std::unique_ptr<multipass::utils::Timer> timer;

    if (parser->isSet("timeout"))
    {
        timer = cmd::make_timer(parser->value("timeout").toInt(), &spinner, cerr,
                                "Timed out waiting for instance to start.");
        timer->start();
    }

    ReturnCode return_code;
    auto streaming_callback = make_iterative_spinner_callback<StartRequest, StartReply>(spinner, *term);
    do
    {
        spinner.start(instance_action_message_for(request.instance_names(), "Starting "));
    } while ((return_code = dispatch(&RpcMethod::start, request, on_success, on_failure, streaming_callback)) ==
             ReturnCode::Retry);

    return return_code;
}

std::string cmd::Start::name() const
{
    return "start";
}

QString cmd::Start::short_help() const
{
    return QStringLiteral("Start instances");
}

QString cmd::Start::description() const
{
    return QStringLiteral("Start the named instances. Exits with return code 0\n"
                          "when the instances start, or with an error code if\n"
                          "any fail to start.");
}

mp::ParseCode cmd::Start::parse_args(mp::ArgParser* parser)
{
    const auto& [description, syntax] =
        petenv_name.isEmpty()
            ? std::make_pair(QString{"Names of instances to start."}, QString{"<name> [<name> ...]"})
            : std::make_pair(QString{"Names of instances to start. If omitted, and without the --all option, '%1' (the "
                                     "configured primary instance name) will be assumed. If '%1' does not exist but is "
                                     "included in a successful start command (either implicitly or explicitly), it is "
                                     "launched automatically (see `launch` for more info)."}
                                 .arg(petenv_name),
                             QString{"[<name> ...]"});

    parser->addPositionalArgument("name", description, syntax);

    QCommandLineOption all_option(all_option_name, "Start all instances");
    parser->addOption(all_option);

    mp::cmd::add_timeout(parser);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
        return status;

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr, /*allow_empty=*/!petenv_name.isEmpty());
    if (parse_code != ParseCode::Ok)
    {
        if (petenv_name.isEmpty() && parser->positionalArguments().isEmpty())
            fmt::print(cerr, "Note: the primary instance is disabled.\n");

        return parse_code;
    }

    try
    {
        request.set_timeout(mp::cmd::parse_timeout(parser));
    }
    catch (const mp::ValidationException& e)
    {
        cerr << "error: " << e.what() << std::endl;
        return ParseCode::CommandLineError;
    }

    request.mutable_instance_names()->CopyFrom(add_instance_names(parser, /*default_name=*/petenv_name.toStdString()));

    return status;
}

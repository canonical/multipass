/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include "stop.h"
#include "common_cli.h"

#include "animated_spinner.h"

#include <multipass/cli/argparser.h>
#include <multipass/constants.h>
#include <multipass/settings.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Stop::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::StopReply& reply) {
        return ReturnCode::Ok;
    };

    AnimatedSpinner spinner{cout};
    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    spinner.start(instance_action_message_for(request.instance_names(), "Stopping "));
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::stop, request, on_success, on_failure);
}

std::string cmd::Stop::name() const { return "stop"; }

QString cmd::Stop::short_help() const
{
    return QStringLiteral("Stop running instances");
}

QString cmd::Stop::description() const
{
    return QStringLiteral("Stop the named instances, if running. Exits with\n"
                          "return code 0 if successful.");
}

mp::ParseCode cmd::Stop::parse_args(mp::ArgParser* parser)
{
    const auto petenv_name = MP_SETTINGS.get(petenv_key);
    parser->addPositionalArgument(
        "name",
        QString{"Names of instances to stop. If omitted, and without the --all option, '%1' will be assumed"}.arg(
            petenv_name),
        "[<name> ...]");

    QCommandLineOption all_option(all_option_name, "Stop all instances");
    QCommandLineOption time_option({"t", "time"}, "Time from now, in minutes, to delay shutdown of the instance",
                                   "time", "0");
    QCommandLineOption cancel_option({"c", "cancel"}, "Cancel a pending delayed shutdown");
    QCommandLineOption force_option({"f", "force"}, "Force switching the instance off");
    parser->addOptions({all_option, time_option, cancel_option, force_option});

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr, /*allow_empty=*/true);
    if (parse_code != ParseCode::Ok)
        return parse_code;

    if (parser->isSet(time_option) && parser->isSet(cancel_option))
    {
        cerr << "Cannot set \'time\' and \'cancel\' options at the same time\n";
        return ParseCode::CommandLineError;
    }

    if (parser->isSet(force_option) && (parser->isSet(time_option) || parser->isSet(cancel_option)))
    {
        cerr << "Cannot set \'force\' along with \'time\' or \'cancel\' options at the same time\n";
        return ParseCode::CommandLineError;
    }

    auto time = parser->value(time_option);

    if (time.startsWith("+"))
    {
        time.remove(0, 1);
    }

    if (!mp::utils::has_only_digits(time.toStdString()))
    {
        cerr << "Time must be in digit form\n";
        return ParseCode::CommandLineError;
    }

    request.set_time_minutes(time.toInt());

    if (parser->isSet(cancel_option))
    {
        request.set_cancel_shutdown(true);
    }

    request.mutable_instance_names()->CopyFrom(add_instance_names(parser, /*default_name=*/petenv_name.toStdString()));

    return status;
}

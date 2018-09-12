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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "stop.h"

#include "animated_spinner.h"

#include <multipass/cli/argparser.h>
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
        cerr << "stop failed: " << status.error_message() << "\n";
        return return_code_for(status.error_code());
    };

    std::string message{"Stopping "};
    if (request.instance_name().empty())
        message.append("all instances");
    else if (request.instance_name().size() > 1)
        message.append("requested instances");
    else
        message.append(request.instance_name().Get(0));
    spinner.start(message);
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
    parser->addPositionalArgument("name", "Names of instances to stop", "<name> [<name> ...]");

    QCommandLineOption all_option("all", "Stop all instances");
    QCommandLineOption time_option({"t", "time"}, "Time from now, in minutes, to delay shutdown of the instance",
                                   "time", "0");
    QCommandLineOption cancel_option({"c", "cancel"}, "Cancel a pending delayed shutdown");
    parser->addOptions({all_option, time_option, cancel_option});

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

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

    if (parser->isSet(time_option) && parser->isSet(cancel_option))
    {
        cerr << "Cannot set \'time\' and \'cancel\' options at the same time\n";
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

    for (const auto& arg : parser->positionalArguments())
    {
        auto entry = request.add_instance_name();
        entry->append(arg.toStdString());
    }

    return status;
}

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
 */

#include "restart.h"
#include "common_cli.h"

#include "animated_spinner.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Restart::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    auto on_success = [](mp::RestartReply& reply) { return ReturnCode::Ok; };

    AnimatedSpinner spinner{cout};
    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    spinner.start(instance_action_message_for(request.instance_names(), "Restarting "));
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::restart, request, on_success, on_failure);
}

std::string cmd::Restart::name() const
{
    return "restart";
}

QString cmd::Restart::short_help() const
{
    return QStringLiteral("Restart instances");
}

QString cmd::Restart::description() const
{
    return QStringLiteral("Restart the named instances. Exits with return\n"
                          "code 0 when the instances restart, or with an\n"
                          "error code if any fail to restart.");
}

mp::ParseCode cmd::Restart::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to restart", "<name> [<name> ...]");

    QCommandLineOption all_option(all_option_name, "Restart all instances");
    parser->addOption(all_option);

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr);
    if (parse_code != ParseCode::Ok)
        return parse_code;

    request.mutable_instance_names()->CopyFrom(add_instance_names(parser));

    return status;
}

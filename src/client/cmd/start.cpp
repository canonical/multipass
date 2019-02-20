/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
#include "common_cli.h"
#include "exec.h"

#include "animated_spinner.h"

#include <multipass/cli/argparser.h>

#include <fmt/ostream.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Start::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    AnimatedSpinner spinner{cout};

    auto on_success = [&spinner](mp::StartReply& reply) {
        spinner.stop();
        return ReturnCode::Ok;
    };

    auto on_failure = [this, &spinner, &parser](grpc::Status& status) {
        spinner.stop();
        auto ret = standard_failure_handler_for(name(), cerr, status);
        if (!status.error_details().empty())
        {
            if (status.error_code() == grpc::StatusCode::ABORTED)
            {
                mp::StartError start_error;
                start_error.ParseFromString(status.error_details());

                if (start_error.error_code() == mp::StartError::INSTANCE_DELETED)
                {
                    fmt::print(cerr,
                               "Use 'recover' to recover the deleted instance or 'purge' to permanently delete the "
                               "instance.\n");
                }
            }
        }

        return ret;
    };

    auto streaming_callback = [&spinner](mp::StartReply& reply) {
        spinner.stop();
        spinner.start(reply.start_message());
    };

    spinner.start(instance_action_message_for(request.instance_names(), "Starting "));
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::start, request, on_success, on_failure, streaming_callback);
}

std::string cmd::Start::name() const { return "start"; }

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
    parser->addPositionalArgument("name", "Names of instances to start", "<name> [<name> ...]");

    QCommandLineOption all_option(all_option_name, "Start all instances");
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

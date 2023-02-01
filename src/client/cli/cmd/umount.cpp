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

#include "umount.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Umount::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::UmountReply& reply) {
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::umount, request, on_success, on_failure);
}

std::string cmd::Umount::name() const { return "umount"; }

std::vector<std::string> cmd::Umount::aliases() const
{
    return {name(), "unmount"};
}

QString cmd::Umount::short_help() const
{
    return QStringLiteral("Unmount a directory from an instance");
}

QString cmd::Umount::description() const
{
    return QStringLiteral("Unmount a directory from an instance.");
}

mp::ParseCode cmd::Umount::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("mount", "Mount points, in <name>[:<path>] format, where <name> "
                                           "are instance names, and optional <path> are mount points. "
                                           "If omitted, all mounts will be removed from the named instances.",
                                  "<mount> [<mount> ...]");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() < 1)
    {
        cerr << "Not enough arguments given\n";
        status = ParseCode::CommandLineError;
    }
    else
    {
        for (const auto& arg : parser->positionalArguments())
        {
            auto parsed_target = arg.split(":");

            auto entry = request.add_target_paths();
            entry->set_instance_name(parsed_target.at(0).toStdString());

            if (parsed_target.count() == 2)
            {
                entry->set_target_path(parsed_target.at(1).toStdString());
            }
        }
    }

    return status;
}

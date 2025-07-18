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

#include "block_attach.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::BlockAttach::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::AttachBlockReply& reply) {
        if (!reply.error_message().empty())
        {
            throw mp::ValidationException{
                fmt::format("Failed to attach block device: {}", reply.error_message())};
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [](grpc::Status& status) {
        throw mp::ValidationException{
            fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::attach_block, request, on_success, on_failure);
}

std::string cmd::BlockAttach::name() const
{
    return "block-attach";
}

QString cmd::BlockAttach::short_help() const
{
    return QStringLiteral("Attach a block device to a VM");
}

QString cmd::BlockAttach::description() const
{
    return QStringLiteral("Attach a block device to a stopped VM instance. The block device\n"
                          "must exist and not be attached to any other VM. The target VM\n"
                          "must be in a stopped state.");
}

mp::ParseCode cmd::BlockAttach::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Name of the block device to attach", "name");

    parser->addPositionalArgument("instance",
                                  "Name of the VM instance to attach the block device to",
                                  "instance");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() != 2)
    {
        throw mp::ValidationException{
            fmt::format("Wrong number of arguments given. Expected 2 ({} {})",
                        parser->positionalArguments()[0].toStdString(),
                        "<instance>")};
    }

    const auto args = parser->positionalArguments();
    request.set_block_name(args.at(0).toStdString());
    request.set_instance_name(args.at(1).toStdString());

    return status;
}

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

#include "block_delete.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::BlockDelete::run(mp::ArgParser* parser)
{
    if (auto ret = parse_args(parser); ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::DeleteBlockReply& reply) {
        if (!reply.error_message().empty())
        {
            throw mp::ValidationException{fmt::format("Failed to delete block device: {}", reply.error_message())};
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [](grpc::Status& status) {
        throw mp::ValidationException{fmt::format("Failed to connect to daemon: {}", status.error_message())};
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::delete_block, request, on_success, on_failure);
}

std::string cmd::BlockDelete::name() const
{
    return "block-delete";
}

QString cmd::BlockDelete::short_help() const
{
    return QStringLiteral("Delete a block device");
}

QString cmd::BlockDelete::description() const
{
    return QStringLiteral("Delete a block device. The device must not be attached to any VM.");
}

mp::ParseCode cmd::BlockDelete::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name",
                                "Name of the block device to delete.",
                                "name");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() != 1)
    {
        throw mp::ValidationException{"block-delete requires one argument: the name of the block device"};
    }

    request.set_name(parser->positionalArguments().first().toStdString());

    return status;
}
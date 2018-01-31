/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include "empty_trash.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::EmptyTrash::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::EmptyTrashReply& reply) {
        return mp::ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "purge failed: " << status.error_message() << "\n";
        return mp::ReturnCode::CommandFail;
    };

    mp::EmptyTrashRequest request;
    return dispatch(&RpcMethod::empty_trash, request, on_success, on_failure);
}

std::string cmd::EmptyTrash::name() const { return "purge"; }

QString cmd::EmptyTrash::short_help() const
{
    return QStringLiteral("Purge all deleted instances permanently");
}

QString cmd::EmptyTrash::description() const
{
    return QStringLiteral("Purge all deleted instances permanently, including all their data.");
}

mp::ParseCode cmd::EmptyTrash::parse_args(mp::ArgParser* parser)
{
    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no arguments\n";
        return ParseCode::CommandLineError;
    }

    return status;
}

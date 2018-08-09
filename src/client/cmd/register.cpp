/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "register.h"

#include <multipass/cli/argparser.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Register::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    auto on_success = [](mp::RegisterReply& reply) { return ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "register failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::registr, request, on_success, on_failure);
}

std::string cmd::Register::name() const
{
    return "register";
}

QString cmd::Register::short_help() const
{
    return QStringLiteral("Register a client");
}

QString cmd::Register::description() const
{
    return QStringLiteral("Register remote client credentials with the local multipass service.\n"
                          "Exits with return code 0 if successful.");
}

mp::ParseCode cmd::Register::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("creds",
                                  "Path to a file containing the remote client credentials.\n"
                                  "On the remote client, use show-creds to obtain credentials.",
                                  "<creds file>");

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    auto num_args = parser->positionalArguments().count();
    if (num_args == 0)
    {
        cerr << "No remote client credentials provided\n";
        return ParseCode::CommandLineError;
    }

    if (num_args > 1)
    {
        cerr << "Too many\n";
        return ParseCode::CommandLineError;
    }

    const auto path = parser->positionalArguments()[0];
    if (!QFile::exists(path))
    {
        cerr << "\"" << path.toStdString() << "\" does not exist\n";
        return ParseCode::CommandLineError;
    }

    auto creds = mp::utils::contents_of(path);
    request.set_cert(creds);
    return status;
}

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

#include "recover.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Recover::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::RecoverReply& reply) {
        return mp::ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "recover failed: " << status.error_message() << "\n";
        return return_code_for(status.error_code());
    };

    return dispatch(&RpcMethod::recover, request, on_success, on_failure);
}

std::string cmd::Recover::name() const { return "recover"; }

QString cmd::Recover::short_help() const
{
    return QStringLiteral("Recover deleted instances");
}

QString cmd::Recover::description() const
{
    return QStringLiteral("Recover deleted instances so they can be used again.");
}

mp::ParseCode cmd::Recover::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to recover", "<name> [<name> ...]");

    QCommandLineOption all_option("all", "Recover all deleted instances");
    parser->addOption(all_option);

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

    for (const auto& arg : parser->positionalArguments())
    {
        auto entry = request.add_instance_name();
        entry->append(arg.toStdString());
    }

    return status;
}

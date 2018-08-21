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

#include "delete.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Delete::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::DeleteReply& reply) { return mp::ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "delete failed: " << status.error_message() << "\n";
        return return_code_for(status.error_code());
    };

    return dispatch(&RpcMethod::delet, request, on_success, on_failure);
}

std::string cmd::Delete::name() const
{
    return "delete";
}

QString cmd::Delete::short_help() const
{
    return QStringLiteral("Delete instances");
}

QString cmd::Delete::description() const
{
    return QStringLiteral("Delete instances, to be purged with the \"purge\" command,\n"
                          "or recovered with the \"recover\" command.");
}

mp::ParseCode cmd::Delete::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to delete", "<name> [<name> ...]");

    QCommandLineOption all_option("all", "Delete all instances");
    parser->addOption(all_option);

    QCommandLineOption purge_option({"p", "purge"}, "Purge instances immediately");
    parser->addOption(purge_option);

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

    if (parser->isSet(purge_option))
    {
        request.set_purge(true);
    }
    return status;
}

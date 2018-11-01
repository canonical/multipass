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

#include "suspend.h"

#include "animated_spinner.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Suspend::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::SuspendReply& reply) { return ReturnCode::Ok; };

    AnimatedSpinner spinner{cout};
    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        cerr << "suspend failed: " << status.error_message() << "\n";
        return return_code_for(status.error_code());
    };

    std::string message{"Suspending "};
    if (request.instance_name().empty())
        message.append("all instances");
    else if (request.instance_name().size() > 1)
        message.append("requested instances");
    else
        message.append(request.instance_name().Get(0));
    spinner.start(message);
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::suspend, request, on_success, on_failure);
}

std::string cmd::Suspend::name() const
{
    return "suspend";
}

QString cmd::Suspend::short_help() const
{
    return QStringLiteral("Suspend running instances");
}

QString cmd::Suspend::description() const
{
    return QStringLiteral("Suspend the named instances, if running. Exits with\n"
                          "return code 0 if successful.");
}

mp::ParseCode cmd::Suspend::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to suspend", "<name> [<name> ...]");

    QCommandLineOption all_option("all", "Suspend all instances");
    parser->addOptions({all_option});

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

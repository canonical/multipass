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

#include <multipass/cli/argparser.h>

#include <cassert>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Restart::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if(ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    Command * stop_cmd = parser->findCommand(QStringLiteral("stop"));
    Command * start_cmd = parser->findCommand(QStringLiteral("start"));
    assert(stop_cmd && start_cmd); /* The restart command is tightly coupled
    with stop and start commands. The alternative would require either
    significant refactoring or code duplication. */

    auto stop_ret = stop_cmd->run(parser);
    return stop_ret == ReturnCode::Ok ? start_cmd->run(parser) : stop_ret; /*
    The argument sequences that are acceptable in a restart command are also
    acceptable to both stop and start commands. */
}

std::string cmd::Restart::name() const { return "restart"; }

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
{   // This code should probably be factored out (for multiple commands)
    parser->addPositionalArgument("name", "Names of instances to restart",
                                  "<name> [<name> ...]");

    QCommandLineOption all_option("all", "Restart all instances");
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

    return ParseCode::Ok; /* We are done with checks and that is all we need in
                             the reset command */
}

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

#include "help.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Help::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    cmd::Command* cmd = parser->findCommand(command);

    if (cmd == nullptr)
    {
        cerr << "Error: Unknown Command: '" << qUtf8Printable(command) << "'\n";
        return ReturnCode::CommandLineError;
    }

    parser->forceCommandHelp();
    cmd->run(parser);

    return ReturnCode::Ok;
}

std::string cmd::Help::name() const { return "help"; }

QString cmd::Help::short_help() const
{
    return QStringLiteral("Display help about a command");
}

QString cmd::Help::description() const
{
    return QStringLiteral("Displays help for the given command.");
}

mp::ParseCode cmd::Help::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("command", "Name of command to display help for", "<command>");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() == 0)
    {
        parser->forceGeneralHelp();
        status = ParseCode::HelpRequested;
    }
    else if (parser->positionalArguments().count() > 1)
    {
        cerr << "Too many arguments given\n";
        status = ParseCode::CommandLineError;
    }
    else
    {
        command = parser->positionalArguments().first();
    }

    return status;
}

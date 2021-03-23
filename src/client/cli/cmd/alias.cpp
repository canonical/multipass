/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "alias.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Alias::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    aliases.add_alias(alias_name, alias_definition);

    // TODO: create symlink/script to directly execute the alias

    return ReturnCode::Ok;
}

std::string cmd::Alias::name() const
{
    return "alias";
}

QString cmd::Alias::short_help() const
{
    return QStringLiteral("Create an alias");
}

QString cmd::Alias::description() const
{
    return QStringLiteral("Create an alias to be executed on a given instance.");
}

mp::ParseCode cmd::Alias::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Name given to the alias being defined", "<name>");
    parser->addPositionalArgument("definition", "Alias definition in the form <instance>:'<command> [<args>]'",
                                  "<definition>");

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    if (parser->positionalArguments().count() != 2)
    {
        cerr << "Wrong number of arguments given\n";
        return ParseCode::CommandLineError;
    }

    QStringList cl_definition = parser->positionalArguments();

    QString definition = cl_definition[1];

    auto colon_pos = definition.indexOf(':');

    if (colon_pos == -1 || colon_pos == definition.size() - 1)
    {
        cerr << "No command given\n";
        return ParseCode::CommandLineError;
    }
    if (colon_pos == 0)
    {
        cerr << "No instance name given\n";
        return ParseCode::CommandLineError;
    }

    alias_name = cl_definition[0].toStdString();

    if (aliases.get_alias(alias_name))
    {
        cerr << fmt::format("Alias '{}' already exists\n", alias_name);
        return ParseCode::CommandLineError;
    }

    std::string instance = definition.left(colon_pos).toStdString();
    QString full_command = definition.right(definition.size() - colon_pos - 1);
    QStringList full_command_split = full_command.split(' ', QString::SkipEmptyParts);

    std::string command = full_command_split[0].toStdString();
    full_command_split.removeFirst();

    std::vector<std::string> arguments;
    for (const auto& arg : full_command_split)
        arguments.push_back(arg.toStdString());

    alias_definition = AliasDefinition{instance, command, arguments};

    return ParseCode::Ok;
}

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
#include <multipass/platform.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Alias::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    try
    {
        MP_PLATFORM.create_alias_script(alias_name, alias_definition);
    }
    catch (std::runtime_error&)
    {
        cerr << fmt::format("Cannot create script for alias '{}'\n", alias_name);
        return ReturnCode::CommandLineError;
    }

    aliases.add_alias(alias_name, alias_definition);

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
    parser->addPositionalArgument("definition", "Alias definition in the form <instance>:<command>", "<definition>");
    parser->addPositionalArgument("name", "Name given to the alias being defined, defaults to <command>", "[<name>]");

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    if (parser->positionalArguments().count() != 1 && parser->positionalArguments().count() != 2)
    {
        cerr << "Wrong number of arguments given\n";
        return ParseCode::CommandLineError;
    }

    QStringList cl_definition = parser->positionalArguments();

    QString definition = cl_definition[0];

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

    std::string instance = definition.left(colon_pos).toStdString();
    std::string command = definition.right(definition.size() - colon_pos - 1).toStdString();

    alias_name = parser->positionalArguments().count() == 1 ? command : cl_definition[1].toStdString();

    if (aliases.get_alias(alias_name))
    {
        cerr << fmt::format("Alias '{}' already exists\n", alias_name);
        return ParseCode::CommandLineError;
    }

    alias_definition = AliasDefinition{instance, command};

    return ParseCode::Ok;
}

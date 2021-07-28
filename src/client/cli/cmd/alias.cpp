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
using RpcMethod = mp::Rpc::Stub;

namespace
{
void show_path_message(std::ostream& os)
{
    os << fmt::format("You'll need to add this to your shell configuration (.bashrc, .zshrc or so) for\n"
                      "aliases to work without prefixing with `multipass`:\n\nPATH={}:$PATH\n",
                      MP_PLATFORM.get_alias_scripts_folder().absolutePath());
}
} // namespace

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
    catch (std::runtime_error& e)
    {
        cerr << fmt::format("Error when creating script for alias: {}\n", e.what());
        return ReturnCode::CommandLineError;
    }

    bool empty_before_add = aliases.empty();

    aliases.add_alias(alias_name, alias_definition);

    if (empty_before_add && aliases.size() == 1)
        show_path_message(cout);

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

    auto instance = definition.left(colon_pos).toStdString();
    bool instance_exists{false};

    list_request.set_verbosity_level(0);
    list_request.set_request_ipv4(false);

    auto on_success = [&instance, &instance_exists](ListReply& reply)
    {
        for (const auto& reply_instance : reply.instances())
            if (instance == reply_instance.name())
            {
                instance_exists = true;
                break;
            }

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return ReturnCode::CommandLineError; };

    if (dispatch(&RpcMethod::list, list_request, on_success, on_failure) == ReturnCode::CommandLineError)
    {
        cerr << "Error retrieving list of instances\n";
        return ParseCode::CommandLineError;
    }

    if (!instance_exists)
    {
        cerr << fmt::format("Instance '{}' does not exist\n", instance);
        return ParseCode::CommandLineError;
    }

    auto command = definition.right(definition.size() - colon_pos - 1).toStdString();

    alias_name = parser->positionalArguments().count() == 1 ? command : cl_definition[1].toStdString();

    if (aliases.get_alias(alias_name))
    {
        cerr << fmt::format("Alias '{}' already exists\n", alias_name);
        return ParseCode::CommandLineError;
    }

    alias_definition = AliasDefinition{instance, command};

    return ParseCode::Ok;
}

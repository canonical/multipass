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

#include "rename.h"

#include "animated_spinner.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCodeVariant cmd::Rename::run(ArgParser* parser)
{
    const auto parscode = parse_args(parser);
    if (parscode != ParseCode::Ok)
    {
        return parser->returnCodeFrom(parscode);
    }

    AnimatedSpinner spinner{cout};
    const auto old_name = rpc_request.instance_name();
    const auto new_name = rpc_request.new_name();

    auto action_on_success =
        [this, &spinner, &old_name, &new_name](RenameReply& reply) -> ReturnCodeVariant {
        spinner.stop();

        std::vector<std::pair<std::string, AliasDefinition>> aliases_to_update;
        for (const auto& [context_name, context] : aliases)
        {
            for (const auto& [alias_name, definition] : context)
            {
                if (definition.instance == old_name)
                {
                    aliases_to_update.emplace_back(alias_name, definition);
                }
            }
        }

        for (const auto& [alias_name, definition] : aliases_to_update)
        {
            aliases.remove_alias(alias_name);
            aliases.add_alias(
                alias_name,
                AliasDefinition{new_name, definition.command, definition.working_directory});
        }

        return ReturnCode::Ok;
    };

    auto action_on_failure = [this, &spinner](grpc::Status& status,
                                              RenameReply& reply) -> ReturnCodeVariant {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    spinner.start("Renaming " + old_name);
    return dispatch(&RpcMethod::rename, rpc_request, action_on_success, action_on_failure);
}

std::string cmd::Rename::name() const
{
    return "rename";
}

QString cmd::Rename::short_help() const
{
    return QStringLiteral("Rename an instance");
}

QString cmd::Rename::description() const
{
    return QStringLiteral("Rename an existing instance.");
}

mp::ParseCode cmd::Rename::parse_args(ArgParser* parser)
{
    parser->addPositionalArgument("current_name",
                                  "The current name of the instance to rename",
                                  "<current_name>");
    parser->addPositionalArgument("new_name", "The new name for the instance", "<new_name>");

    const auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
    {
        return status;
    }

    const auto number_of_positional_arguments = parser->positionalArguments().count();
    if (number_of_positional_arguments < 2)
    {
        cerr << "Please provide the current and new names for the instance.\n";
        return ParseCode::CommandLineError;
    }

    if (number_of_positional_arguments > 2)
    {
        cerr << "Too many arguments.\n";
        return ParseCode::CommandLineError;
    }

    const auto current_name = parser->positionalArguments()[0];
    const auto new_name = parser->positionalArguments()[1];

    rpc_request.set_instance_name(current_name.toStdString());
    rpc_request.set_new_name(new_name.toStdString());
    rpc_request.set_verbosity_level(parser->verbosityLevel());

    return ParseCode::Ok;
}

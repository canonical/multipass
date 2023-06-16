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

#include "unalias.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/platform.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Unalias::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto old_active_context = aliases.active_context_name();

    for (const auto& [context_from, alias_only] : aliases_to_remove)
    {
        aliases.set_active_context(context_from);
        aliases.remove_alias(alias_only); // We know removal won't fail because the alias exists.
        try
        {
            MP_PLATFORM.remove_alias_script(context_from + "." + alias_only);

            if (!aliases.exists_alias(alias_only))
            {
                MP_PLATFORM.remove_alias_script(alias_only);
            }
        }
        catch (std::runtime_error& e)
        {
            cerr << fmt::format("Warning: '{}' when removing alias script for {}.{}\n", e.what(), context_from,
                                alias_only);
        }
    }

    aliases.set_active_context(old_active_context);

    return ReturnCode::Ok;
}

std::string cmd::Unalias::name() const
{
    return "unalias";
}

QString cmd::Unalias::short_help() const
{
    return QStringLiteral("Remove aliases");
}

QString cmd::Unalias::description() const
{
    return QStringLiteral("Remove aliases");
}

mp::ParseCode cmd::Unalias::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of aliases to remove", "<name> [<name> ...]");

    QCommandLineOption all_option(all_option_name, "Remove all aliases from current context");
    parser->addOption(all_option);

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr, false);
    if (parse_code != ParseCode::Ok)
        return parse_code;

    std::vector<std::string> bad_aliases;

    if (parser->isSet(all_option_name))
    {
        const auto& active_context_name = aliases.active_context_name();
        const auto& active_context = aliases.get_active_context();

        for (auto definition_it = active_context.cbegin(); definition_it != active_context.cend(); ++definition_it)
            aliases_to_remove.emplace_back(active_context_name, definition_it->first);
    }
    else
    {
        for (const auto& arg : parser->positionalArguments())
        {
            auto arg_str = arg.toStdString();

            auto context_and_alias = aliases.get_context_and_alias(arg_str);

            if (context_and_alias)
                aliases_to_remove.emplace_back(context_and_alias->first, context_and_alias->second);
            else
                bad_aliases.push_back(arg_str);
        }
    }

    if (!bad_aliases.empty())
    {
        cerr << fmt::format("Nonexistent {}: {}.\n", bad_aliases.size() == 1 ? "alias" : "aliases",
                            fmt::join(bad_aliases, ", "));

        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}

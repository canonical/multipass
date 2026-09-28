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

#include "autocomplete.h"

#include <multipass/cli/argparser.h>

#include <string>
#include <vector>

namespace multipass::cmd
{

std::string AutoComplete::name() const
{
    // Prefixing with '__' to exclude the command from the CLI help text.
    // This is a hack until the ArgParser is properly reworked.
    return "__autocomplete";
}

QString AutoComplete::short_help() const
{
    return "";
}

QString AutoComplete::description() const
{
    return "";
}

ReturnCodeVariant AutoComplete::run(ArgParser* parser)
{
    const auto prefix_option = QCommandLineOption{
        "prefix",
        "Whether the last given argument is the element to autocomplete"};
    parser->addOption(prefix_option);

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
    {
        return parser->returnCodeFrom(status);
    }

    const auto& arguments = parser->positionalArguments();
    if (arguments.empty())
    {
        return parser->returnCodeFrom(ParseCode::CommandLineError);
    }

    const auto& command_name = arguments[0];
    const auto* command = parser->findCommand(command_name);
    if (command == nullptr)
    {
        return parser->returnCodeFrom(ParseCode::CommandLineError);
    }

    const auto last_is_prefix = parser->isSet(prefix_option);
    const auto prefix = last_is_prefix ? arguments.back().toStdString() : "";

    const auto previous_end = last_is_prefix ? arguments.size() - 1 : arguments.size();
    auto previous = std::vector<std::string>{};
    for (auto i = 1; i < previous_end; ++i)
    {
        previous.emplace_back(arguments[i].toStdString());
    }

    const auto proposals = command->autocomplete(prefix, previous);
    for (const auto& proposal : proposals)
    {
        cout << proposal << " ";
    }
    cout << std::endl;

    return parser->returnCodeFrom(ParseCode::Ok);
}

} // namespace multipass::cmd

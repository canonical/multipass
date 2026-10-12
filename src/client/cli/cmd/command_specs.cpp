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

#include <multipass/cli/command_specs.h>

#include <multipass/cli/argparser.h>
#include <multipass/cli/autocompleter.h>

#include <QCommandLineOption>
#include <QString>
#include <QStringList>

namespace multipass
{

void CommandSpecs::configure_parser(ArgParser& parser) const
{
    for (const auto& option : optional_parameters)
    {
        auto names = QStringList{QString::fromStdString(option.name)};
        for (const auto& alias : option.aliases)
        {
            names << QString::fromStdString(alias);
        }

        parser.addOption(
            QCommandLineOption{std::move(names), QString::fromStdString(option.description)});
    }

    for (const auto& param : positional_parameters)
    {
        parser.addPositionalArgument(QString::fromStdString(param.name),
                                     QString::fromStdString(param.description),
                                     QString::fromStdString(param.syntax));
    }
}

AutoCompleter CommandSpecs::make_completer() const
{
    auto completer_specs = AutoCompleteCommandSpecs{};
    completer_specs.last_repeatable = last_repeatable;

    for (const auto& option : optional_parameters)
    {
        auto& option_specs = completer_specs.optional_parameters.try_emplace("--" + option.name)
                                 .first->second;
        option_specs.repeatable = option.repeatable;
        option_specs.excluded_options = option.excluded_options;
        option_specs.excluded_parameters = option.excluded_parameters;

        for (const auto& p : option.parameters)
        {
            auto& param_specs = option_specs.parameters.emplace_back();
            param_specs.arguments_provider = p.arguments_provider;
        }
    }

    for (const auto& param : positional_parameters)
    {
        auto& param_specs = completer_specs.positional_parameters.emplace_back();
        param_specs.arguments_provider = param.arguments_provider;
    }

    return AutoCompleter{std::move(completer_specs)};
}

} // namespace multipass

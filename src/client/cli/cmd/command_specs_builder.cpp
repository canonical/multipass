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

#include "command_specs_builder.h"

#include <multipass/cli/command_specs.h>

#include <stdexcept>

namespace multipass
{

OptionBuilder::OptionBuilder(CommandSpecs& specs, size_t idx) : _specs{specs}, _idx{idx}
{
}

const std::string& OptionBuilder::id() const
{
    return _specs.optional_parameters[_idx].name;
}

OptionBuilder& OptionBuilder::add_alias(std::string alias)
{
    option().aliases.push_back(std::move(alias));
    return *this;
}

OptionBuilder& OptionBuilder::set_description(std::string text)
{
    option().description = std::move(text);
    return *this;
}

OptionBuilder& OptionBuilder::set_repeatable(bool repeatable)
{
    option().repeatable = repeatable;
    return *this;
}

OptionBuilder& OptionBuilder::add_parameter(std::vector<std::string> arguments)
{
    return add_parameter([arguments = std::move(arguments)]() { return arguments; });
}

OptionBuilder& OptionBuilder::add_parameter(
    std::function<std::vector<std::string>()> arguments_provider)
{
    auto& param = option().parameters.emplace_back();
    param.arguments_provider = std::move(arguments_provider);
    return *this;
}

OptionSpecs& OptionBuilder::option()
{
    return _specs.optional_parameters[_idx];
}

ParameterBuilder::ParameterBuilder(CommandSpecs& specs, size_t idx) : _specs{specs}, _idx{idx}
{
}

size_t ParameterBuilder::id() const
{
    return _idx;
}

ParameterBuilder& ParameterBuilder::set_name(std::string name)
{
    parameter().name = std::move(name);
    return *this;
}

ParameterBuilder& ParameterBuilder::set_description(std::string text)
{
    parameter().description = std::move(text);
    return *this;
}

ParameterBuilder& ParameterBuilder::set_syntax(std::string syntax)
{
    parameter().syntax = std::move(syntax);
    return *this;
}

ParameterBuilder& ParameterBuilder::set_arguments(std::vector<std::string> parameters)
{
    return set_arguments_provider([parameters = std::move(parameters)]() { return parameters; });
}

ParameterBuilder& ParameterBuilder::set_arguments_provider(
    std::function<std::vector<std::string>()> provider)
{
    parameter().arguments_provider = std::move(provider);
    return *this;
}

ParameterSpecs& ParameterBuilder::parameter()
{
    return _specs.positional_parameters[_idx];
}

CommandSpecsBuilder& CommandSpecsBuilder::set_name(std::string name)
{
    _specs.name = std::move(name);
    return *this;
}

CommandSpecsBuilder& CommandSpecsBuilder::set_short_help(std::string short_help)
{
    _specs.short_help = std::move(short_help);
    return *this;
}

CommandSpecsBuilder& CommandSpecsBuilder::set_description(std::string description)
{
    _specs.description = std::move(description);
    return *this;
}

OptionBuilder CommandSpecsBuilder::add_optional_argument(std::string option)
{
    auto idx = _specs.optional_parameters.size();
    _option_indices[option] = idx;

    auto& specs = _specs.optional_parameters.emplace_back();
    specs.name = std::move(option);

    return OptionBuilder{_specs, idx};
}

ParameterBuilder CommandSpecsBuilder::add_positional_argument()
{
    auto idx = _specs.positional_parameters.size();
    _specs.positional_parameters.emplace_back();

    return ParameterBuilder{_specs, idx};
}

CommandSpecsBuilder& CommandSpecsBuilder::set_repeatable_last_parameter(bool repeatable)
{
    _specs.last_repeatable = repeatable;
    return *this;
}

CommandSpecsBuilder& CommandSpecsBuilder::add_mutual_exclusion(std::string_view option_1,
                                                               std::string_view option_2)
{
    if (auto it1 = _option_indices.find(option_1); it1 != _option_indices.end())
    {
        if (auto it2 = _option_indices.find(option_2); it2 != _option_indices.end())
        {
            _specs.optional_parameters[it1->second].excluded_options.emplace(
                std::string{"--"} += option_2);

            _specs.optional_parameters[it2->second].excluded_options.emplace(
                std::string{"--"} += option_1);
        }
    }

    return *this;
}

CommandSpecsBuilder& CommandSpecsBuilder::add_mutual_exclusion(size_t parameter,
                                                               std::string_view option)
{
    if (parameter < _specs.positional_parameters.size())
    {
        if (auto it = _option_indices.find(option); it != _option_indices.end())
        {
            _specs.optional_parameters[it->second].excluded_parameters.emplace(parameter);
        }
    }

    return *this;
}

CommandSpecs CommandSpecsBuilder::build() &&
{
    if (_specs.name.empty())
    {
        throw std::logic_error{"Command name must be set"};
    }

    if (_specs.short_help.empty())
    {
        throw std::logic_error{"Short help must be set"};
    }

    if (_specs.description.empty())
    {
        throw std::logic_error{"Description must be set"};
    }

    return std::move(_specs);
}

} // namespace multipass

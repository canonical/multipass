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

#pragma once

#include <multipass/cli/command_specs.h>
#include <multipass/disabled_copy_move.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace multipass
{

class OptionBuilder
{
public:
    explicit OptionBuilder(CommandSpecs& specs, size_t idx);

    const std::string& id() const;

    OptionBuilder& add_alias(std::string alias);
    OptionBuilder& set_description(std::string text);

    OptionBuilder& set_repeatable(bool repeatable);

    OptionBuilder& add_parameter(std::vector<std::string> arguments);
    OptionBuilder& add_parameter(std::function<std::vector<std::string>()> arguments_provider);

private:
    OptionSpecs& option();

    CommandSpecs& _specs;
    size_t _idx = 0;
};

class ParameterBuilder
{
public:
    explicit ParameterBuilder(CommandSpecs& specs, size_t idx);

    size_t id() const;

    ParameterBuilder& set_name(std::string name);
    ParameterBuilder& set_description(std::string text);
    ParameterBuilder& set_syntax(std::string syntax);

    ParameterBuilder& set_arguments(std::vector<std::string> parameters);
    ParameterBuilder& set_arguments_provider(std::function<std::vector<std::string>()> provider);

private:
    ParameterSpecs& parameter();

    CommandSpecs& _specs;
    size_t _idx = 0;
};

class CommandSpecsBuilder : private DisabledCopyMove
{
public:
    CommandSpecsBuilder& set_name(std::string name);
    CommandSpecsBuilder& set_short_help(std::string short_help);
    CommandSpecsBuilder& set_description(std::string description);

    OptionBuilder add_optional_argument(std::string option);
    ParameterBuilder add_positional_argument();

    CommandSpecsBuilder& set_repeatable_last_parameter(bool repeatable);

    CommandSpecsBuilder& add_mutual_exclusion(std::string_view option_1, std::string_view option_2);
    CommandSpecsBuilder& add_mutual_exclusion(size_t parameter_idx, std::string_view option);

    CommandSpecs build() &&;

private:
    CommandSpecs _specs;
    std::map<std::string, size_t, std::less<>> _option_indices;
};

} // namespace multipass

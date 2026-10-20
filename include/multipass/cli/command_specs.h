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

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace multipass
{

class ArgParser;
class AutoCompleter;

struct ParameterSpecs
{
    std::string name;
    std::string description;
    std::string syntax;

    std::function<std::vector<std::string>()> arguments_provider;
};

struct OptionSpecs
{
    std::string name;
    std::vector<std::string> aliases;
    std::string description;

    std::vector<ParameterSpecs> parameters;

    std::set<std::string> excluded_options;
    std::set<size_t> excluded_parameters;

    bool repeatable = false;
};

struct CommandSpecs
{
    void configure_parser(ArgParser& parser) const;
    AutoCompleter make_completer() const;

    std::string name;
    std::string short_help;
    std::string description;

    std::vector<OptionSpecs> optional_parameters;
    std::vector<ParameterSpecs> positional_parameters;

    bool last_repeatable = false;
};

} // namespace multipass

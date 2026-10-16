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

#include <multipass/cli/autocompleter.h>

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace multipass
{

namespace
{

void complete_parameter(std::vector<std::string>& completions,
                        const AutoCompleteParameterSpecs& parameter,
                        const std::vector<std::string>& previous)
{
    for (const auto& completion : parameter.arguments_provider())
    {
        if (std::ranges::find(previous, completion) == previous.end())
        {
            completions.push_back(completion);
        }
    }
}

void complete_option(std::vector<std::string>& completions,
                     const AutoCompleteOptionSpecs& option,
                     size_t next_parameters_idx)
{
    if (next_parameters_idx < option.parameters.size())
    {
        complete_parameter(completions, option.parameters[next_parameters_idx], {});
    }
}

} // namespace

AutoCompleter::AutoCompleter(AutoCompleteCommandSpecs specs)
    : _specs{std::move(specs)}, _exclusions_by_param{_specs.positional_parameters.size()}
{
    for (const auto& [option, option_spec] : _specs.optional_parameters)
    {
        if (!option.starts_with("--"))
        {
            throw std::logic_error{"Option " + option + " must start with '--'"};
        }

        for (const auto& other : option_spec.excluded_options)
        {
            if (other == option)
            {
                throw std::logic_error{"Option " + option + " cannot exclude itself"};
            }

            if (!_specs.optional_parameters.contains(other))
            {
                throw std::logic_error{"Invalid exclusion set for " + option + ": option '" +
                                       other + "' does not exist"};
            }
        }

        for (const auto& param : option_spec.excluded_parameters)
        {
            if (param >= _exclusions_by_param.size())
            {
                throw std::out_of_range{"Invalid exclusion set for " + option + ": parameter #" +
                                        std::to_string(param) + " does not exist"};
            }

            _exclusions_by_param[param].emplace(option);
        }
    }
}

void AutoCompleter::complete(std::vector<std::string>& completions,
                             const std::vector<std::string>& previous) const
{
    auto next_parameter_idx = size_t{0};
    auto last_option_it = _specs.optional_parameters.end();
    auto last_option_params_count = size_t{0};
    auto skip_options = false;

    for (auto idx = size_t{0}; idx < previous.size(); ++idx)
    {
        const auto& prev = previous[idx];
        if (prev == "--")
        {
            next_parameter_idx += previous.size() - idx - 1;
            last_option_it = _specs.optional_parameters.end();
            last_option_params_count = 0;
            skip_options = true;
            break;
        }

        if (prev.starts_with("--") || (prev.starts_with("-") && prev.size() == 2))
        {
            last_option_it = _specs.optional_parameters.find(option_key(prev));
            last_option_params_count = last_option_it != _specs.optional_parameters.end()
                                         ? last_option_it->second.parameters.size()
                                         : 0;
        }
        else if (last_option_params_count > 0)
        {
            --last_option_params_count;
        }
        else
        {
            ++next_parameter_idx;
        }
    }

    if (last_option_params_count > 0)
    {
        auto& last_option = last_option_it->second;
        complete_option(completions,
                        last_option,
                        last_option.parameters.size() - last_option_params_count);
        return;
    }

    if (!skip_options)
    {
        for (const auto& [key, option] : _specs.optional_parameters)
        {
            if (!is_excluded(key, previous, next_parameter_idx))
            {
                completions.push_back(key);
            }
        }
    }

    if (_specs.last_repeatable && next_parameter_idx >= _specs.positional_parameters.size())
    {
        next_parameter_idx = _specs.positional_parameters.size() - 1;
    }

    if (next_parameter_idx < _specs.positional_parameters.size() &&
        !is_excluded(next_parameter_idx, previous))
    {
        complete_parameter(completions, _specs.positional_parameters[next_parameter_idx], previous);
    }
}

std::vector<std::string> AutoCompleter::complete(const std::vector<std::string>& previous) const
{
    auto completions = std::vector<std::string>{};
    complete(completions, previous);
    return completions;
}

const std::string& AutoCompleter::option_key(const std::string& option) const
{
    const auto alias_it = _specs.option_aliases.find(option);
    return alias_it != _specs.option_aliases.end() ? alias_it->second : option;
}

bool AutoCompleter::is_excluded(const std::string& option,
                                const std::vector<std::string>& previous,
                                size_t parameters_count) const
{
    auto it = _specs.optional_parameters.find(option);
    if (it == _specs.optional_parameters.end())
    {
        return false;
    }

    const auto& option_specs = it->second;

    for (auto i = size_t{0}; i < parameters_count; ++i)
    {
        if (option_specs.excluded_parameters.contains(i))
        {
            return true;
        }
    }

    for (const auto& prev : previous)
    {
        const auto& prev_key = option_key(prev);
        if (prev_key == option && !option_specs.repeatable)
        {
            return true;
        }

        const auto prev_it = _specs.optional_parameters.find(prev_key);
        if (prev_it != _specs.optional_parameters.end())
        {
            if (option_specs.excluded_options.contains(prev_key) ||
                prev_it->second.excluded_options.contains(option))
            {
                return true;
            }
        }
    }

    return false;
}

bool AutoCompleter::is_excluded(size_t parameter, const std::vector<std::string>& previous) const
{
    if (parameter < _exclusions_by_param.size())
    {
        const auto& exclusions = _exclusions_by_param[parameter];
        return std::ranges::any_of(previous, [this, &exclusions](const auto& p) {
            return exclusions.contains(option_key(p));
        });
    }

    return false;
}

} // namespace multipass

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

namespace multipass
{

void AutoCompleter::Parameter::complete(std::vector<std::string>& completions,
                                        const std::vector<std::string>& previous) const
{
    for (const auto& completion : provider())
    {
        if (std::ranges::find(previous, completion) == previous.end())
        {
            completions.push_back(completion);
        }
    }
}

void AutoCompleter::Option::complete(std::vector<std::string>& completions,
                                     size_t next_parameters_idx) const
{
    if (next_parameters_idx < parameters.size())
    {
        parameters[next_parameters_idx].complete(completions, {});
    }
}

std::string_view AutoCompleter::add_option(std::string_view option, bool is_repeatable)
{
    auto option_id = std::string{"--"};
    option_id += option;

    auto [it, inserted] = _options.try_emplace(std::move(option_id));
    it->second.is_repeatable = is_repeatable;
    return it->first;
}

void AutoCompleter::add_option_parameter(std::string_view option,
                                         std::function<std::vector<std::string>()> provider)
{
    if (auto it = _options.find(option); it != _options.end())
    {
        it->second.parameters.push_back({std::move(provider)});
    }
}

void AutoCompleter::add_option_parameter(std::string_view option, std::vector<std::string> values)
{
    return add_option_parameter(option, [values = std::move(values)]() { return values; });
}

size_t AutoCompleter::add_parameter(std::function<std::vector<std::string>()> provider)
{
    auto idx = _parameters.size();
    _parameters.push_back({std::move(provider)});
    return idx;
}

size_t AutoCompleter::add_parameter(std::vector<std::string> values)
{
    return add_parameter([values = std::move(values)]() { return values; });
}

void AutoCompleter::set_repeat_last_parameter(bool do_repeat)
{
    _do_repeat_last = do_repeat;
}

void AutoCompleter::set_mutual_exclusion(std::string_view option_1, std::string_view option_2)
{
    _exclusions_by_option[std::string{option_1}].emplace(option_2);
    _exclusions_by_option[std::string{option_2}].emplace(option_1);
}

void AutoCompleter::set_mutual_exclusion(std::string_view option, size_t parameter)
{
    _exclusions_by_param[parameter].emplace(option);
}

void AutoCompleter::complete(std::vector<std::string>& completions,
                             const std::vector<std::string>& previous) const
{
    const auto [last_option, last_option_idx] = find_last_option(previous);
    if (last_option && last_option->parameters.size() > previous.size() - last_option_idx)
    {
        last_option->complete(completions, previous.size() - last_option_idx);
        return;
    }

    for (const auto& [key, option] : _options)
    {
        if ((option.is_repeatable || std::ranges::find(previous, key) == previous.end()) &&
            !is_excluded(key, previous))
        {
            completions.push_back(key);
        }
    }

    const auto first_parameter_idx = last_option ? last_option_idx + last_option->parameters.size()
                                                 : 0;
    const auto next_parameter_idx = previous.size() - first_parameter_idx;
    if (next_parameter_idx < _parameters.size())
    {
        if (!is_excluded(next_parameter_idx, previous))
        {
            _parameters[next_parameter_idx].complete(completions, previous);
        }
    }
    else if (_do_repeat_last && !_parameters.empty())
    {
        if (!is_excluded(_parameters.size() - 1, previous))
        {
            _parameters.back().complete(completions, previous);
        }
    }
}

std::vector<std::string> AutoCompleter::complete(const std::vector<std::string>& previous) const
{
    std::vector<std::string> completions;
    complete(completions, previous);
    return completions;
}

const AutoCompleter::Option* AutoCompleter::get_option(std::string_view key) const
{
    auto it = _options.find(key);
    return it != _options.end() ? &it->second : nullptr;
}

const std::pair<const AutoCompleter::Option*, size_t> AutoCompleter::find_last_option(
    const std::vector<std::string>& previous) const
{
    auto it = std::find_if(previous.rbegin(), previous.rend(), [](const std::string& key) {
        return key.starts_with("--");
    });

    if (it != previous.rend())
    {
        return {get_option(*it), static_cast<size_t>(std::distance(it, previous.rend()))};
    }

    return {nullptr, 0};
}

bool AutoCompleter::is_excluded(std::string_view option,
                                const std::vector<std::string>& previous) const
{
    auto parameter_idx = size_t{0};

    for (const auto& prev : previous)
    {
        const auto is_option = prev.starts_with("--");
        if (is_option)
        {
            auto it = _exclusions_by_option.find(prev);
            if (it != _exclusions_by_option.end() && it->second.contains(option))
            {
                return true;
            }
        }
        else
        {
            auto it = _exclusions_by_param.find(parameter_idx++);
            if (it != _exclusions_by_param.end() && it->second.contains(option))
            {
                return true;
            }
        }
    }

    return false;
}

bool AutoCompleter::is_excluded(size_t parameter, const std::vector<std::string>& previous) const
{
    auto it = _exclusions_by_param.find(parameter);
    if (it != _exclusions_by_param.end())
    {
        return std::ranges::any_of(previous,
                                   [&it](const auto& p) { return it->second.contains(p); });
    }

    return false;
}

} // namespace multipass

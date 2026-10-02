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
    _exclusions_by_param.emplace_back();
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
    auto next_parameter_idx = size_t{0};
    auto last_option_it = _options.end();
    auto last_option_params_count = size_t{0};
    for (auto idx = size_t{0}; idx < previous.size(); ++idx)
    {
        if (previous[idx].starts_with("--"))
        {
            last_option_it = _options.find(previous[idx]);
            last_option_params_count = last_option_it != _options.end()
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
        last_option.complete(completions, last_option.parameters.size() - last_option_params_count);
        return;
    }

    for (const auto& [key, option] : _options)
    {
        if ((option.is_repeatable || std::ranges::find(previous, key) == previous.end()) &&
            !is_excluded(key, previous, next_parameter_idx))
        {
            completions.push_back(key);
        }
    }

    if (_do_repeat_last && next_parameter_idx >= _parameters.size())
    {
        next_parameter_idx = _parameters.size() - 1;
    }

    if (next_parameter_idx < _parameters.size() && !is_excluded(next_parameter_idx, previous))
    {
        _parameters[next_parameter_idx].complete(completions, previous);
    }
}

std::vector<std::string> AutoCompleter::complete(const std::vector<std::string>& previous) const
{
    std::vector<std::string> completions;
    complete(completions, previous);
    return completions;
}

bool AutoCompleter::is_excluded(std::string_view option,
                                const std::vector<std::string>& previous,
                                size_t parameters_count) const
{
    for (auto i = size_t{0}; i < parameters_count && i < _exclusions_by_param.size(); ++i)
    {
        if (_exclusions_by_param[i].contains(option))
        {
            return true;
        }
    }

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
    }

    return false;
}

bool AutoCompleter::is_excluded(size_t parameter, const std::vector<std::string>& previous) const
{
    if (parameter < _exclusions_by_param.size())
    {
        const auto& exclusions = _exclusions_by_param[parameter];
        return std::ranges::any_of(previous,
                                   [&exclusions](const auto& p) { return exclusions.contains(p); });
    }

    return false;
}

} // namespace multipass

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

#include <multipass/disabled_copy_move.h>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace multipass
{

struct AutoCompleteParameterSpecs
{
    std::function<std::vector<std::string>()> arguments_provider;
};

struct AutoCompleteOptionSpecs
{
    std::vector<AutoCompleteParameterSpecs> parameters = {};

    std::set<std::string> excluded_options = {};
    std::set<size_t> excluded_parameters = {};

    bool repeatable = false;
};

struct AutoCompleteCommandSpecs
{
    std::map<std::string, AutoCompleteOptionSpecs> optional_parameters = {};
    std::vector<AutoCompleteParameterSpecs> positional_parameters = {};
    bool last_repeatable = false;
};

class AutoCompleter : private DisabledCopyMove
{
public:
    explicit AutoCompleter(AutoCompleteCommandSpecs specs);

    void complete(std::vector<std::string>& completions,
                  const std::vector<std::string>& previous) const;

    std::vector<std::string> complete(const std::vector<std::string>& previous) const;

private:
    bool is_excluded(const std::string& option,
                     const std::vector<std::string>& previous,
                     size_t parameters_count) const;
    bool is_excluded(size_t parameter, const std::vector<std::string>& previous) const;

    AutoCompleteCommandSpecs _specs;
    std::vector<std::set<std::string>> _exclusions_by_param;
};

} // namespace multipass

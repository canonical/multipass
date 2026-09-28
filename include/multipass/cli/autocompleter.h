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

class AutoCompleter : private DisabledCopyMove
{
    struct Parameter
    {
        void complete(std::vector<std::string>& completions,
                      const std::vector<std::string>& previous) const;

        std::function<std::vector<std::string>()> provider;
    };

    struct Option
    {
        void complete(std::vector<std::string>& completions, size_t next_parameter_idx) const;

        std::vector<Parameter> parameters;
        bool is_repeatable = false;
    };

public:
    std::string_view add_option(std::string_view option, bool is_repeatable = false);

    void add_option_parameter(std::string_view option,
                              std::function<std::vector<std::string>()> provider);
    void add_option_parameter(std::string_view option, std::vector<std::string> values);

    size_t add_parameter(std::function<std::vector<std::string>()> provider);
    size_t add_parameter(std::vector<std::string> values);

    void set_repeat_last_parameter(bool do_repeat);

    void set_mutual_exclusion(std::string_view option_1, std::string_view option_2);
    void set_mutual_exclusion(std::string_view option, size_t parameter);

    void complete(std::vector<std::string>& completions,
                  const std::vector<std::string>& previous) const;

    std::vector<std::string> complete(const std::vector<std::string>& previous) const;

private:
    const Option* get_option(std::string_view key) const;

    const std::pair<const Option*, size_t> find_last_option(
        const std::vector<std::string>& previous) const;

    bool is_excluded(std::string_view option, const std::vector<std::string>& previous) const;
    bool is_excluded(size_t parameter, const std::vector<std::string>& previous) const;

    std::vector<Parameter> _parameters;
    std::map<std::string, Option, std::less<>> _options;
    bool _do_repeat_last = false;

    std::map<int, std::set<std::string, std::less<>>> _exclusions_by_param;
    std::map<std::string, std::set<std::string, std::less<>>> _exclusions_by_option;
};

} // namespace multipass

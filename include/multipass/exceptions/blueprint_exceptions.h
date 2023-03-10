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

#ifndef MULTIPASS_BLUEPRINT_EXCEPTIONS_H
#define MULTIPASS_BLUEPRINT_EXCEPTIONS_H

#include <multipass/format.h>

#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

namespace multipass
{

class BlueprintMinimumException : public std::runtime_error
{
public:
    BlueprintMinimumException(const std::string& type, const YAML::Node& limits_min_resource_node,
                              const std::string& blueprint_name)
        : runtime_error(fmt::format("Requested {} is less than Blueprint minimum.", type) + "\n" +
                        query_min_resource_info(limits_min_resource_node, blueprint_name))
    {
    }

private:
    static std::string left_trim_and_replace_last_comma_with_and(const std::string& input_str)
    {
        if (input_str.empty())
            return {};

        // trim leftest comma first and replace the last comma with " and" if that last comma exist
        auto left_trimmed_str = input_str.substr(1, input_str.size() - 1);
        const auto last_comma_pos = left_trimmed_str.find_last_of(',');
        return last_comma_pos != std::string::npos ? left_trimmed_str.replace(last_comma_pos, 1, " and")
                                                   : left_trimmed_str;
    }

    [[nodiscard]] static std::string query_min_resource_info(const YAML::Node& limits_min_resource_node,
                                                             const std::string& blueprint_name)
    {
        const auto& limits_min_cpu_node = limits_min_resource_node["min-cpu"];
        // limits_min_cpu_node.as<int>() can not throw here because query_min_resource_info function is only called in
        // BlueprintMinimumException branch
        const std::string min_cpu_info_str =
            limits_min_cpu_node ? fmt::format(", {} CPUs", limits_min_cpu_node.as<int>()) : std::string();

        const auto& limits_min_mem_node = limits_min_resource_node["min-mem"];
        const std::string min_mem_info_str =
            limits_min_mem_node ? fmt::format(", {} of memory", limits_min_mem_node.as<std::string>()) : std::string();

        const auto& limits_min_disk_node = limits_min_resource_node["min-disk"];
        const std::string min_disk_info_str =
            limits_min_disk_node ? fmt::format(", {} of disk space", limits_min_disk_node.as<std::string>())
                                 : std::string();

        const std::string whole_min_resource_info_str = fmt::format(
            "The {} requires at least{}.", blueprint_name,
            left_trim_and_replace_last_comma_with_and(min_cpu_info_str + min_mem_info_str + min_disk_info_str));

        return whole_min_resource_info_str;
    }
};

class InvalidBlueprintException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class IncompatibleBlueprintException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
} // namespace multipass
#endif // MULTIPASS_BLUEPRINT_EXCEPTIONS_H

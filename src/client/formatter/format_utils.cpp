/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/cli/format_utils.h>
#include <multipass/cli/formatter.h>

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>

namespace mp = multipass;

namespace
{
template <typename T>
std::unique_ptr<mp::Formatter> make_entry()
{
    return std::make_unique<T>();
}

auto make_map()
{
    std::map<std::string, std::unique_ptr<mp::Formatter>> map;
    map.emplace("table", make_entry<mp::TableFormatter>());
    map.emplace("json", make_entry<mp::JsonFormatter>());
    map.emplace("csv", make_entry<mp::CSVFormatter>());
    map.emplace("yaml", make_entry<mp::YamlFormatter>());
    return map;
}

const std::map<std::string, std::unique_ptr<mp::Formatter>> formatters{make_map()};
} // namespace

std::string mp::format::status_string_for(const mp::InstanceStatus& status)
{
    std::string status_val;

    switch (status.status())
    {
    case mp::InstanceStatus::RUNNING:
        status_val = "RUNNING";
        break;
    case mp::InstanceStatus::STOPPED:
        status_val = "STOPPED";
        break;
    case mp::InstanceStatus::DELETED:
        status_val = "DELETED";
        break;
    case mp::InstanceStatus::STARTING:
        status_val = "STARTING";
        break;
    case mp::InstanceStatus::RESTARTING:
        status_val = "RESTARTING";
        break;
    case mp::InstanceStatus::DELAYED_SHUTDOWN:
        status_val = "DELAYED SHUTDOWN";
        break;
    case mp::InstanceStatus::SUSPENDING:
        status_val = "SUSPENDING";
        break;
    case mp::InstanceStatus::SUSPENDED:
        status_val = "SUSPENDED";
        break;
    default:
        status_val = "UNKNOWN";
        break;
    }
    return status_val;
}

mp::Formatter* mp::format::formatter_for(const std::string& format)
{
    auto entry = formatters.find(format);
    if (entry != formatters.end())
        return entry->second.get();
    return nullptr;
}

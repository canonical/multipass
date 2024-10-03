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

#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/format_utils.h>
#include <multipass/cli/formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>

#include <iomanip>
#include <locale>
#include <sstream>

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
const std::set<std::string> unwanted_aliases{"ubuntu", "default"};

} // namespace

std::string mp::format::status_string_for(const mp::InstanceStatus& status)
{
    std::string status_val;

    switch (status.status())
    {
    case mp::InstanceStatus::RUNNING:
        status_val = "Running";
        break;
    case mp::InstanceStatus::STOPPED:
        status_val = "Stopped";
        break;
    case mp::InstanceStatus::DELETED:
        status_val = "Deleted";
        break;
    case mp::InstanceStatus::STARTING:
        status_val = "Starting";
        break;
    case mp::InstanceStatus::RESTARTING:
        status_val = "Restarting";
        break;
    case mp::InstanceStatus::DELAYED_SHUTDOWN:
        status_val = "Delayed Shutdown";
        break;
    case mp::InstanceStatus::SUSPENDING:
        status_val = "Suspending";
        break;
    case mp::InstanceStatus::SUSPENDED:
        status_val = "Suspended";
        break;
    default:
        status_val = "Unknown";
        break;
    }
    return status_val;
}

std::string mp::format::image_string_for(const mp::FindReply_AliasInfo& alias)
{
    return alias.remote_name().empty() ? alias.alias() : fmt::format("{}:{}", alias.remote_name(), alias);
}

mp::Formatter* mp::format::formatter_for(const std::string& format)
{
    auto entry = formatters.find(format);
    if (entry != formatters.end())
        return entry->second.get();
    return nullptr;
}

void mp::format::filter_aliases(google::protobuf::RepeatedPtrField<multipass::FindReply_AliasInfo>& aliases)
{
    for (auto i = aliases.size() - 1; i >= 0; i--)
    {
        if (aliases.size() == 1) // Keep at least the first entry
            break;
        if (aliases[i].alias().length() == 1)
            aliases.DeleteSubrange(i, 1);
        else if (unwanted_aliases.find(aliases[i].alias()) != unwanted_aliases.end())
            aliases.DeleteSubrange(i, 1);
    }
}

mp::FormatUtils::FormatUtils(const Singleton<FormatUtils>::PrivatePass& pass) noexcept
    : Singleton<FormatUtils>::Singleton{pass}
{
}

std::string mp::FormatUtils::convert_to_user_locale(const google::protobuf::Timestamp& timestamp) const
{
    std::ostringstream oss;
    oss.imbue(std::locale(""));

    std::time_t t = google::protobuf::util::TimeUtil::TimestampToTimeT(timestamp);
    oss << std::put_time(std::localtime(&t), "%c %Z");

    return oss.str();
}

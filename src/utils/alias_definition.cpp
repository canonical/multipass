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

#include <multipass/alias_definition.h>
#include <multipass/format.h>
#include <multipass/json_utils.h>

#include <boost/json.hpp>

namespace json = boost::json;
namespace mp = multipass;

namespace
{
void check_working_directory_string(const std::string& dir)
{
    if (dir != "map" && dir != "default")
        throw std::runtime_error(fmt::format("invalid working_directory string \"{}\"", dir));
}
} // namespace

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::AliasDefinition& alias)
{
    check_working_directory_string(alias.working_directory);

    json = {{"instance", alias.instance},
            {"command", alias.command},
            {"working-directory", alias.working_directory}};
}

mp::AliasDefinition mp::tag_invoke(const boost::json::value_to_tag<mp::AliasDefinition>&,
                                   const boost::json::value& json)
{
    const auto& obj = json.as_object();

    std::string working_directory;
    if (auto wd = obj.if_contains("working-directory"); wd && !wd->is_null())
        working_directory = value_to<std::string>(*wd);
    if (working_directory.empty())
        working_directory = "default";
    check_working_directory_string(working_directory);

    return {.instance = value_to<std::string>(obj.at("instance")),
            .command = value_to<std::string>(obj.at("command")),
            .working_directory = std::move(working_directory)};
}

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::AliasContext& context)
{
    json = boost::json::value_from(context, SortJsonKeys{});
}

mp::AliasContext mp::tag_invoke(const boost::json::value_to_tag<mp::AliasContext>&,
                                const boost::json::value& json)
{
    AliasContext result;
    for (const auto& [key, value] : json.as_object())
    {
        if (value.as_object().empty())
            continue;
        result.emplace(key, value_to<AliasDefinition>(value));
    }
    return result;
}

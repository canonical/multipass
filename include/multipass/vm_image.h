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

#include <multipass/json_utils.h>
#include <multipass/path.h>

#include <vector>

#include <boost/json.hpp>

namespace multipass
{
class VMImage
{
public:
    Path image_path;
    std::string id;
    std::string original_release;
    std::string current_release;
    std::string release_date;
    std::string os;
    std::vector<std::string> aliases;
};

inline void tag_invoke(const boost::json::value_from_tag&,
                       boost::json::value& json,
                       const VMImage& image)
{
    boost::json::array aliases;
    for (const auto& alias : image.aliases)
        aliases.push_back(boost::json::object{{"alias", alias}});

    json = {{"path", image.image_path.toStdString()},
            {"id", image.id},
            {"original_release", image.original_release},
            {"current_release", image.current_release},
            {"release_date", image.release_date},
            {"os", image.os},
            {"aliases", aliases}};
}

inline VMImage tag_invoke(const boost::json::value_to_tag<VMImage>&, const boost::json::value& json)
{
    std::vector<std::string> aliases;
    for (const auto& entry : json.at("aliases").as_array())
        aliases.push_back(value_to<std::string>(entry.at("alias")));

    return {value_to<QString>(json.at("path")),
            value_to<std::string>(json.at("id")),
            lookup_or<std::string>(json, "original_release", ""),
            lookup_or<std::string>(json, "current_release", ""),
            lookup_or<std::string>(json, "release_date", ""),
            lookup_or<std::string>(json, "os", ""),
            aliases};
}
} // namespace multipass

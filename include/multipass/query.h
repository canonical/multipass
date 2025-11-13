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

#include <string>

#include <boost/json.hpp>

namespace multipass
{
class Query
{
public:
    enum Type
    {
        Alias,
        LocalFile,
        HttpDownload
    };

    std::string name;
    std::string release;
    bool persistent;
    std::string remote_name;
    Type query_type;
    bool allow_unsupported{false};
};

inline void tag_invoke(const boost::json::value_from_tag&,
                       boost::json::value& json,
                       const Query& query)
{
    json = {{"release", query.release},
            {"persistent", query.persistent},
            {"remote_name", query.remote_name},
            {"query_type", static_cast<int>(query.query_type)}};
}

inline Query tag_invoke(const boost::json::value_to_tag<Query>&, const boost::json::value& json)
{
    return {
        "",
        value_to<std::string>(json.at("release")),
        value_to<bool>(json.at("persistent")),
        lookup_or<std::string>(json, "remote_name", ""),
        static_cast<Query::Type>(lookup_or<int>(json, "query_type", 0)),
    };
}
} // namespace multipass

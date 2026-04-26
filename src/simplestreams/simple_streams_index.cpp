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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *              Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/json_utils.h>
#include <multipass/simple_streams_index.h>

#include <boost/json.hpp>

#include <stdexcept>
#include <string_view>

namespace mp = multipass;

mp::SimpleStreamsIndex mp::SimpleStreamsIndex::fromJson(const QByteArray& json)
{
    auto doc = boost::json::parse(std::string_view{json});
    if (!doc.is_object())
        throw std::runtime_error("invalid index object");

    auto index = doc.at("index").as_object();

    for (auto&& [_, value] : index)
    {
        if (lookup_or<std::string>(value, "datatype", "") == "image-downloads")
        {
            return {lookup_or<QString>(value, "path", ""),
                    lookup_or<QString>(value, "updated", "")};
        }
    }

    throw std::runtime_error("no image-downloads entry found");
}

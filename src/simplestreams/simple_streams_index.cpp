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

#include <multipass/simple_streams_index.h>

#include <nlohmann/json.hpp>

#include <stdexcept>

using json = nlohmann::json;
namespace mp = multipass;

namespace
{
json parse_index(std::string_view json)
{
    auto doc = json::parse(json);
    if (!doc.is_object())
        throw std::runtime_error("invalid index object");

    auto index = doc.at("index");
    if (index.empty())
        throw std::runtime_error("No index found");

    return index;
}
} // namespace

mp::SimpleStreamsIndex mp::SimpleStreamsIndex::fromJson(const QByteArray& json)
{
    auto index = parse_index(json.toStdString());

    for (auto&& [_, value] : index.items())
    {
        if (value.at("datatype") == "image-downloads")
        {
            auto path_entry = value.at("path").get<std::string>();
            auto date_entry = value.at("updated").get<std::string>();
            return {QString::fromStdString(path_entry), QString::fromStdString(date_entry)};
        }
    }

    throw std::runtime_error("no image-downloads entry found");
}

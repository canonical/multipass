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

#include <QString>
#include <QStringList>

#include <string>
#include <vector>

#include <boost/algorithm/string/trim.hpp>
#include <boost/json.hpp>

#include <multipass/exceptions/unsupported_arch_exception.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>

namespace multipass
{
struct VMImageInfo
{
    std::vector<std::string> aliases;
    std::string os;
    std::string release;
    std::string release_title;
    std::string release_codename;
    bool supported;
    std::string image_location;
    std::string id;
    std::string stream_location;
    std::string version;
    int64_t size;
    bool verify;

    friend inline bool operator==(const VMImageInfo& a, const VMImageInfo& b) = default;
};

struct ArchContext
{
    std::string arch;
};

inline VMImageInfo tag_invoke(const boost::json::value_to_tag<VMImageInfo>&,
                              const boost::json::value& json,
                              const ArchContext& arch)
{
    const auto arch_json = json.at("items").try_at(arch.arch);
    if (arch_json.has_error())
        throw UnsupportedArchException(arch.arch);

    std::vector<std::string> aliases;
    for (auto& alias : utils::split(value_to<std::string>(json.at("aliases")), ","))
    {
        boost::trim(alias);
        if (!alias.empty())
            aliases.push_back(std::move(alias));
    }

    return {aliases,
            value_to<std::string>(json.at("os")),
            value_to<std::string>(json.at("release")),
            value_to<std::string>(json.at("release_codename")),
            value_to<std::string>(json.at("release_title")),
            true,
            value_to<std::string>(arch_json->at("image_location")),
            value_to<std::string>(arch_json->at("id")),
            "",
            value_to<std::string>(arch_json->at("version")),
            lookup_or<int>(*arch_json, "size", -1),
            true};
}
} // namespace multipass

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

#include <boost/algorithm/string/trim.hpp>

#include <multipass/exceptions/unsupported_arch_exception.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>
#include <multipass/vm_image_info.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

mp::VMImageInfo mp::tag_invoke(const boost::json::value_to_tag<mp::VMImageInfo>&,
                               const boost::json::value& json,
                               const mp::ArchContext& arch)
{
    const auto arch_json = json.at("items").try_at(arch.arch);
    if (arch_json.has_error())
        throw mp::UnsupportedArchException(arch.arch);

    std::vector<std::string> aliases;
    for (auto& alias : mpu::split(value_to<std::string>(json.at("aliases")), ","))
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

std::unordered_map<std::string, const mp::VMImageInfo*> mp::map_aliases_to_vm_info(
    const std::vector<mp::VMImageInfo>& images)
{
    std::unordered_map<std::string, const mp::VMImageInfo*> map;

    for (const auto& image : images)
    {
        map[image.id] = &image;
        for (const auto& alias : image.aliases)
        {
            map[alias] = &image;
        }
    }

    return map;
}

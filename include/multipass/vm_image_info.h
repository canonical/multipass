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

#include <string>
#include <vector>

#include <boost/json.hpp>

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

VMImageInfo tag_invoke(const boost::json::value_to_tag<VMImageInfo>&,
                       const boost::json::value& json,
                       const ArchContext& arch);

std::unordered_map<std::string, const VMImageInfo*> map_aliases_to_vm_info(
    const std::vector<VMImageInfo>& images);
} // namespace multipass

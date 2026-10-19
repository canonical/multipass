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

#include <multipass/vm_mount.h>

#include <multipass/file_ops.h>

namespace mp = multipass;

namespace
{
auto print_mappings(const std::unordered_map<int, std::unordered_set<int>>& dup_id_map,
                    const std::unordered_map<int, std::unordered_set<int>>& dup_rev_id_map)
{
    std::string retval;

    for (const auto& pair : dup_id_map)
    {
        retval += fmt::format("{}: [", pair.first);
        for (const auto& mapping : pair.second)
            retval += fmt::format("{}:{}, ", pair.first, mapping);

        retval = retval.substr(0, retval.size() - 2);
        retval += "]; ";
    }

    for (const auto& pair : dup_rev_id_map)
    {
        retval += fmt::format("{}: [", pair.first);
        for (const auto& mapping : pair.second)
            retval += fmt::format("{}:{}, ", mapping, pair.first);

        retval = retval.substr(0, retval.size() - 2);
        retval += "]; ";
    }

    return retval.substr(0, retval.size() - 2);
}
} // namespace

mp::VMMount::VMMount(const std::string& sourcePath,
                     id_mappings gidMappings,
                     id_mappings uidMappings,
                     MountType mountType)
    : source_path(MP_FILEOPS.weakly_canonical(sourcePath).string()),
      gid_mappings(std::move(gidMappings)),
      uid_mappings(std::move(uidMappings)),
      mount_type(mountType)
{
    fmt::memory_buffer errors;

    if (const auto& [dup_uid_map, dup_rev_uid_map] = mp::unique_id_mappings(uid_mappings);
        !dup_uid_map.empty() || !dup_rev_uid_map.empty())
    {
        fmt::format_to(std::back_inserter(errors),
                       "\nuids: {}",
                       print_mappings(dup_uid_map, dup_rev_uid_map));
    }

    if (const auto& [dup_gid_map, dup_rev_gid_map] = mp::unique_id_mappings(gid_mappings);
        !dup_gid_map.empty() || !dup_rev_gid_map.empty())
    {
        fmt::format_to(std::back_inserter(errors),
                       "\ngids: {}",
                       print_mappings(dup_gid_map, dup_rev_gid_map));
    }

    if (errors.size())
        throw std::runtime_error(fmt::format("Mount cannot apply mapping with duplicate ids:{}",
                                             fmt::to_string(errors)));
}

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::VMMount& mount)
{
    json = {{"source_path", mount.source_path},
            {"gid_mappings", boost::json::value_from(mount.gid_mappings, IdMappingType::gid)},
            {"uid_mappings", boost::json::value_from(mount.uid_mappings, IdMappingType::uid)},
            {"mount_type", static_cast<int>(mount.mount_type)}};
}

mp::VMMount mp::tag_invoke(const boost::json::value_to_tag<mp::VMMount>&,
                           const boost::json::value& json)
{
    return {value_to<std::string>(json.at("source_path")),
            value_to<id_mappings>(json.at("gid_mappings"), IdMappingType::gid),
            value_to<id_mappings>(json.at("uid_mappings"), IdMappingType::uid),
            VMMount::MountType(value_to<int>(json.at("mount_type")))};
}

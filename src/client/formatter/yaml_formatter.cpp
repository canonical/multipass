/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/cli/yaml_formatter.h>
#include <multipass/cli/format_utils.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <yaml-cpp/yaml.h>

#include <locale>

namespace mp = multipass;

namespace
{
std::string emit_node(YAML::Node& node)
{
    YAML::Emitter emitter;
    emitter.SetIndent(2);
    emitter << node;
    if (!emitter.good())
        throw std::runtime_error{"Failed to emit list info"};

    emitter << YAML::Newline;
    return emitter.c_str();
}
}
std::string mp::YamlFormatter::format(const InfoReply& reply) const
{
    YAML::Node info_node;

    info_node["errors"].push_back(YAML::Null);

    for (const auto& info : format::sorted(reply.info()))
    {
        YAML::Node instance_node;

        instance_node["state"] = mp::format::status_string_for(info.instance_status());
        instance_node["image_hash"] = info.id();
        instance_node["image_release"] = info.image_release();
        if (info.current_release().empty())
            instance_node["release"] = YAML::Null;
        else
            instance_node["release"] = info.current_release();

        if (!info.load().empty())
        {
            // The VM returns load info in the default C locale
            auto current_loc = std::locale();
            std::locale::global(std::locale("C"));
            auto loads = mp::utils::split(info.load(), " ");
            for (const auto& entry : loads)
                instance_node["load"].push_back(std::stod(entry));
            std::locale::global(current_loc);
        }

        YAML::Node disk;
        disk["used"] = YAML::Null;
        disk["total"] = YAML::Null;
        if (!info.disk_usage().empty())
            disk["used"] = info.disk_usage();
        if (!info.disk_total().empty())
            disk["total"] = info.disk_total();

        // TODO: disk name should come from daemon
        YAML::Node disk_node;
        disk_node["sda1"] = disk;
        instance_node["disks"].push_back(disk_node);

        YAML::Node memory;
        memory["usage"] = YAML::Null;
        memory["total"] = YAML::Null;
        if (!info.memory_usage().empty())
            memory["usage"] = std::stoll(info.memory_usage());
        if (!info.memory_total().empty())
            memory["total"] = std::stoll(info.memory_total());

        instance_node["memory"] = memory;

        if (!info.ipv4().empty())
            instance_node["ipv4"].push_back(info.ipv4());

        YAML::Node mounts;
        for (const auto& mount : info.mount_info().mount_paths())
        {
            YAML::Node mount_node;

            for (const auto uid_map : mount.mount_maps().uid_map())
            {
                mount_node["uid_mappings"].push_back(
                    fmt::format("{}:{}", std::to_string(uid_map.first),
                                (uid_map.second == mp::default_id) ? "default" : std::to_string(uid_map.second)));
            }
            for (const auto gid_map : mount.mount_maps().gid_map())
            {
                mount_node["gid_mappings"].push_back(
                    fmt::format("{}:{}", std::to_string(gid_map.first),
                                (gid_map.second == mp::default_id) ? "default" : std::to_string(gid_map.second)));
            }

            mount_node["source_path"] = mount.source_path();
            mounts[mount.target_path()] = mount_node;
        }
        instance_node["mounts"] = mounts;

        info_node[info.name()].push_back(instance_node);
    }
    return emit_node(info_node);
}

std::string mp::YamlFormatter::format(const ListReply& reply) const
{
    YAML::Node list;

    for (const auto& instance : format::sorted(reply.instances()))
    {
        YAML::Node instance_node;
        instance_node["state"] = mp::format::status_string_for(instance.instance_status());

        instance_node["ipv4"].push_back(instance.ipv4());
        instance_node["release"] = instance.current_release();

        list[instance.name()].push_back(instance_node);
    }

    return emit_node(list);
}

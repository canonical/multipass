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

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/client_common.h>
#include <multipass/cli/format_utils.h>
#include <multipass/cli/yaml_formatter.h>
#include <multipass/format.h>
#include <multipass/utils.h>
#include <multipass/yaml_node_utils.h>

#include <yaml-cpp/yaml.h>

#include <locale>

namespace mp = multipass;
namespace mpu = multipass::utils;

namespace
{
template <typename ImageInfo>
std::map<std::string, YAML::Node> format_images(const ImageInfo& images_info)
{
    std::map<std::string, YAML::Node> images_node;

    for (const auto& image : images_info)
    {
        YAML::Node image_node;
        image_node["aliases"] = std::vector<std::string>{};

        auto aliases = image.aliases_info();
        mp::format::filter_aliases(aliases);

        for (auto alias = aliases.cbegin() + 1; alias != aliases.cend(); alias++)
            image_node["aliases"].push_back(alias->alias());

        image_node["os"] = image.os();
        image_node["release"] = image.release();
        image_node["version"] = image.version();
        image_node["remote"] = aliases[0].remote_name();

        images_node[mp::format::image_string_for(aliases[0])] = image_node;
    }

    return images_node;
}

YAML::Node generate_snapshot_details(const mp::DetailedInfoItem& item)
{
    const auto& snapshot_details = item.snapshot_info();
    const auto& fundamentals = snapshot_details.fundamentals();
    YAML::Node snapshot_node;

    snapshot_node["size"] = snapshot_details.size().empty() ? YAML::Node() : YAML::Node(snapshot_details.size());
    snapshot_node["cpu_count"] = item.cpu_count();
    snapshot_node["disk_space"] = item.disk_total();
    snapshot_node["memory_size"] = item.memory_total();

    YAML::Node mounts;
    for (const auto& mount : item.mount_info().mount_paths())
    {
        YAML::Node mount_node;
        mount_node["source_path"] = mount.source_path();
        mounts[mount.target_path()] = mount_node;
    }
    snapshot_node["mounts"] = mounts;

    snapshot_node["created"] = MP_FORMAT_UTILS.convert_to_user_locale(fundamentals.creation_timestamp());
    snapshot_node["parent"] = fundamentals.parent().empty() ? YAML::Node() : YAML::Node(fundamentals.parent());

    snapshot_node["children"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& child : snapshot_details.children())
        snapshot_node["children"].push_back(child);

    snapshot_node["comment"] = fundamentals.comment().empty() ? YAML::Node() : YAML::Node(fundamentals.comment());

    return snapshot_node;
}

YAML::Node generate_instance_details(const mp::DetailedInfoItem& item)
{
    const auto& instance_details = item.instance_info();
    YAML::Node instance_node;

    instance_node["state"] = mp::format::status_string_for(item.instance_status());

    if (instance_details.has_num_snapshots())
        instance_node["snapshot_count"] = instance_details.num_snapshots();

    instance_node["image_hash"] = instance_details.id();
    instance_node["image_release"] = instance_details.image_release();
    instance_node["release"] =
        instance_details.current_release().empty() ? YAML::Node() : YAML::Node(instance_details.current_release());
    instance_node["cpu_count"] = item.cpu_count().empty() ? YAML::Node() : YAML::Node(item.cpu_count());

    if (!instance_details.load().empty())
    {
        // The VM returns load info in the default C locale
        auto current_loc = std::locale();
        std::locale::global(std::locale("C"));
        auto loads = mp::utils::split(instance_details.load(), " ");
        for (const auto& entry : loads)
            instance_node["load"].push_back(entry);
        std::locale::global(current_loc);
    }

    YAML::Node disk;
    disk["used"] = instance_details.disk_usage().empty() ? YAML::Node() : YAML::Node(instance_details.disk_usage());
    disk["total"] = item.disk_total().empty() ? YAML::Node() : YAML::Node(item.disk_total());

    // TODO: disk name should come from daemon
    YAML::Node disk_node;
    disk_node["sda1"] = disk;
    instance_node["disks"].push_back(disk_node);

    YAML::Node memory;
    memory["usage"] = instance_details.memory_usage().empty() ? YAML::Node()
                                                              : YAML::Node(std::stoll(instance_details.memory_usage()));
    memory["total"] = item.memory_total().empty() ? YAML::Node() : YAML::Node(std::stoll(item.memory_total()));
    instance_node["memory"] = memory;

    instance_node["ipv4"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& ip : instance_details.ipv4())
        instance_node["ipv4"].push_back(ip);

    YAML::Node mounts;
    for (const auto& mount : item.mount_info().mount_paths())
    {
        YAML::Node mount_node;

        for (const auto& uid_mapping : mount.mount_maps().uid_mappings())
        {
            auto host_uid = uid_mapping.host_id();
            auto instance_uid = uid_mapping.instance_id();

            mount_node["uid_mappings"].push_back(
                fmt::format("{}:{}",
                            std::to_string(host_uid),
                            (instance_uid == mp::default_id) ? "default" : std::to_string(instance_uid)));
        }
        for (const auto& gid_mapping : mount.mount_maps().gid_mappings())
        {
            auto host_gid = gid_mapping.host_id();
            auto instance_gid = gid_mapping.instance_id();

            mount_node["gid_mappings"].push_back(
                fmt::format("{}:{}",
                            std::to_string(host_gid),
                            (instance_gid == mp::default_id) ? "default" : std::to_string(instance_gid)));
        }

        mount_node["source_path"] = mount.source_path();
        mounts[mount.target_path()] = mount_node;
    }
    instance_node["mounts"] = mounts;

    return instance_node;
}

std::string generate_instances_list(const mp::InstancesList& instance_list)
{
    YAML::Node list;

    for (const auto& instance : mp::format::sorted(instance_list.instances()))
    {
        YAML::Node instance_node;
        instance_node["state"] = mp::format::status_string_for(instance.instance_status());

        instance_node["ipv4"] = YAML::Node(YAML::NodeType::Sequence);
        for (const auto& ip : instance.ipv4())
            instance_node["ipv4"].push_back(ip);

        instance_node["release"] =
            instance.current_release().empty() ? "Not Available" : fmt::format("Ubuntu {}", instance.current_release());

        list[instance.name()].push_back(instance_node);
    }

    return mpu::emit_yaml(list);
}

std::string generate_snapshots_list(const mp::SnapshotsList& snapshot_list)
{
    YAML::Node info_node;

    for (const auto& item : mp::format::sorted(snapshot_list.snapshots()))
    {
        const auto& snapshot = item.fundamentals();
        YAML::Node instance_node;
        YAML::Node snapshot_node;

        snapshot_node["parent"] = snapshot.parent().empty() ? YAML::Node() : YAML::Node(snapshot.parent());
        snapshot_node["comment"] = snapshot.comment().empty() ? YAML::Node() : YAML::Node(snapshot.comment());

        instance_node[snapshot.snapshot_name()].push_back(snapshot_node);
        info_node[item.name()].push_back(instance_node);
    }

    return mpu::emit_yaml(info_node);
}
} // namespace

std::string mp::YamlFormatter::format(const InfoReply& reply) const
{
    YAML::Node info_node;

    info_node["errors"].push_back(YAML::Null);

    for (const auto& info : mp::format::sorted(reply.details()))
    {
        if (info.has_instance_info())
        {
            info_node[info.name()].push_back(generate_instance_details(info));
        }
        else
        {
            assert(info.has_snapshot_info() && "either one of instance or snapshot details should be populated");

            YAML::Node snapshot_node;
            snapshot_node[info.snapshot_info().fundamentals().snapshot_name()] = generate_snapshot_details(info);

            info_node[info.name()][0]["snapshots"].push_back(snapshot_node);
        }
    }

    return mpu::emit_yaml(info_node);
}

std::string mp::YamlFormatter::format(const ListReply& reply) const
{
    std::string output;

    if (reply.has_instance_list())
    {
        output = generate_instances_list(reply.instance_list());
    }
    else
    {
        assert(reply.has_snapshot_list() && "eitherr one of instances or snapshots should be populated");
        output = generate_snapshots_list(reply.snapshot_list());
    }

    return output;
}

std::string mp::YamlFormatter::format(const NetworksReply& reply) const
{
    YAML::Node list;

    for (const auto& interface : format::sorted(reply.interfaces()))
    {
        YAML::Node interface_node;
        interface_node["type"] = interface.type();
        interface_node["description"] = interface.description();

        list[interface.name()].push_back(interface_node);
    }

    return mpu::emit_yaml(list);
}

std::string mp::YamlFormatter::format(const FindReply& reply) const
{
    YAML::Node find;
    find["errors"] = std::vector<YAML::Node>{};
    find["blueprints"] = format_images(reply.blueprints_info());
    find["images"] = format_images(reply.images_info());

    return mpu::emit_yaml(find);
}

std::string mp::YamlFormatter::format(const VersionReply& reply, const std::string& client_version) const
{
    YAML::Node version;
    version["multipass"] = client_version;

    if (!reply.version().empty())
    {
        version["multipassd"] = reply.version();

        if (mp::cmd::update_available(reply.update_info()))
        {
            YAML::Node update;
            update["title"] = reply.update_info().title();
            update["description"] = reply.update_info().description();
            update["url"] = reply.update_info().url();

            version["update"] = update;
        }
    }

    return mpu::emit_yaml(version);
}

std::string mp::YamlFormatter::format(const mp::AliasDict& aliases) const
{
    YAML::Node aliases_list, aliases_node;

    for (const auto& [context_name, context_contents] : sort_dict(aliases))
    {
        YAML::Node context_node;

        for (const auto& [name, def] : sort_dict(context_contents))
        {
            YAML::Node alias_node;
            alias_node["alias"] = name;
            alias_node["command"] = def.command;
            alias_node["instance"] = def.instance;
            alias_node["working-directory"] = def.working_directory;

            context_node.push_back(alias_node);
        }

        aliases_node[context_name] = context_node;
    }

    aliases_list["active_context"] = aliases.active_context_name();
    aliases_list["aliases"] = aliases_node;

    return mpu::emit_yaml(aliases_list);
}

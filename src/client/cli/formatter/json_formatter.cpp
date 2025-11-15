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

#include <multipass/cli/client_common.h>
#include <multipass/cli/format_utils.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>

#include <boost/json.hpp>

namespace mp = multipass;

namespace
{
boost::json::value format_images(
    const google::protobuf::RepeatedPtrField<mp::FindReply_ImageInfo>& images_info)
{
    boost::json::object result;
    for (const auto& image : images_info)
    {
        auto aliases = image.aliases();
        mp::format::filter_aliases(aliases);

        boost::json::array aliases_json;
        for (int i = 1; i < aliases.size(); ++i)
            aliases_json.emplace_back(aliases[i]);

        result.emplace(mp::format::image_string_for(image.remote_name(), aliases[0]),
                       boost::json::object{{"os", image.os()},
                                           {"release", image.release()},
                                           {"version", image.version()},
                                           {"aliases", std::move(aliases_json)},
                                           {"remote", image.remote_name()}});
    }
    return result;
}

boost::json::object generate_snapshot_details(const mp::DetailedInfoItem& item)
{
    const auto& snapshot_details = item.snapshot_info();
    const auto& fundamentals = snapshot_details.fundamentals();

    boost::json::object mounts;
    for (const auto& mount : item.mount_info().mount_paths())
        mounts.emplace(mount.target_path(),
                       boost::json::object{{"source_path", mount.source_path()}});

    return {{"size", snapshot_details.size()},
            {"cpu_count", item.cpu_count()},
            {"disk_space", item.disk_total()},
            {"memory_size", item.memory_total()},
            {"mounts", std::move(mounts)},
            {"created", MP_FORMAT_UTILS.convert_to_user_locale(fundamentals.creation_timestamp())},
            {"parent", fundamentals.parent()},
            {"children", boost::json::value_from(snapshot_details.children())},
            {"comment", fundamentals.comment()}};
}

boost::json::object generate_instance_details(const mp::DetailedInfoItem& item)
{
    const auto& instance_details = item.instance_info();

    boost::json::object instance_info = {
        {"state", mp::format::status_string_for(item.instance_status())},
        {"image_hash", instance_details.id()},
        {"image_release", instance_details.image_release()},
        {"release", instance_details.current_release()},
        {"cpu_count", item.cpu_count()}};

    if (instance_details.has_num_snapshots())
        instance_info.emplace("snapshot_count", std::to_string(instance_details.num_snapshots()));

    boost::json::array load;
    if (!instance_details.load().empty())
    {
        auto loads = mp::utils::split(instance_details.load(), " ");
        for (const auto& entry : loads)
            load.push_back(std::stod(entry));
    }
    instance_info.emplace("load", std::move(load));

    boost::json::object disk;
    if (!instance_details.disk_usage().empty())
        disk.emplace("used", instance_details.disk_usage());
    if (!item.disk_total().empty())
        disk.emplace("total", item.disk_total());

    // TODO: disk name should come from daemon
    instance_info.emplace("disks", boost::json::object{{"sda1", std::move(disk)}});

    boost::json::object memory;
    if (!instance_details.memory_usage().empty())
        memory.emplace("used", std::stoll(instance_details.memory_usage()));
    if (!item.memory_total().empty())
        memory.emplace("total", std::stoll(item.memory_total()));
    instance_info.emplace("memory", std::move(memory));

    instance_info.emplace("ipv4", boost::json::value_from(instance_details.ipv4()));

    boost::json::object mounts;
    for (const auto& mount : item.mount_info().mount_paths())
    {
        boost::json::array mount_uids;
        boost::json::array mount_gids;

        auto mount_maps = mount.mount_maps();
        for (auto i = 0; i < mount_maps.uid_mappings_size(); ++i)
        {
            auto uid_map_pair = mount_maps.uid_mappings(i);
            auto host_uid = uid_map_pair.host_id();
            auto instance_uid = uid_map_pair.instance_id();

            if (instance_uid == mp::default_id)
                mount_uids.emplace_back(fmt::format("{}:default", host_uid));
            else
                mount_uids.emplace_back(fmt::format("{}:{}", host_uid, instance_uid));
        }
        for (auto i = 0; i < mount_maps.gid_mappings_size(); ++i)
        {
            auto gid_map_pair = mount_maps.gid_mappings(i);
            auto host_gid = gid_map_pair.host_id();
            auto instance_gid = gid_map_pair.instance_id();

            if (instance_gid == mp::default_id)
                mount_gids.emplace_back(fmt::format("{}:default", host_gid));
            else
                mount_gids.emplace_back(fmt::format("{}:{}", host_gid, instance_gid));
        }

        mounts.emplace(mount.target_path(),
                       boost::json::object{{"uid_mappings", std::move(mount_uids)},
                                           {"gid_mappings", std::move(mount_gids)},
                                           {"source_path", mount.source_path()}});
    }
    instance_info.emplace("mounts", std::move(mounts));

    return instance_info;
}

boost::json::value generate_instances_list(const mp::InstancesList& instance_list)
{
    boost::json::array instances;
    for (const auto& instance : instance_list.instances())
    {
        std::string release =
            instance.current_release().empty()
                ? "Not Available"
                : mp::utils::trim(fmt::format("{} {}", instance.os(), instance.current_release()));
        instances.push_back({
            {"name", instance.name()},
            {"state", mp::format::status_string_for(instance.instance_status())},
            {"ipv4", boost::json::value_from(instance.ipv4())},
            {"release", std::move(release)},
        });
    }

    return {{"list", std::move(instances)}};
}

boost::json::value generate_snapshots_list(const mp::SnapshotsList& snapshot_list)
{
    boost::json::object info_obj;
    for (const auto& item : snapshot_list.snapshots())
    {
        const auto& snapshot = item.fundamentals();
        boost::json::object snapshot_obj = {{"parent", snapshot.parent()},
                                            {"comment", snapshot.comment()}};

        if (auto it = info_obj.find(item.name()); it != info_obj.end())
        {
            auto& obj = it->value().as_object();
            obj.emplace(snapshot.snapshot_name(), snapshot_obj);
        }
        else
        {
            info_obj.emplace(item.name(),
                             boost::json::object{{snapshot.snapshot_name(), snapshot_obj}});
        }
    }

    return {{"errors", boost::json::array{}}, {"info", std::move(info_obj)}};
}
} // namespace

std::string mp::JsonFormatter::format(const InfoReply& reply) const
{
    boost::json::object info_obj;
    for (const auto& info : reply.details())
    {
        if (info.has_instance_info())
        {
            auto instance_details = generate_instance_details(info);
            if (auto instance = info_obj.if_contains(info.name()))
            {
                // Some instance details already exist, so merge the values
                auto& obj = instance->as_object();
                for (const auto& [key, value] : instance_details)
                {
                    assert(!obj.contains(key) && "key already exists");
                    obj.emplace(key, value);
                }
            }
            else
            {
                // Nothing for the instance so far, so insert normally
                info_obj.emplace(info.name(), std::move(instance_details));
            }
        }
        else if (info.has_snapshot_info())
        {
            auto snapshot_details = generate_snapshot_details(info);

            // Get the instance and snapshot objects, or create them if they don't exist.
            using pair = boost::json::object::value_type;
            auto& instance_obj = info_obj.insert(pair{info.name(), boost::json::object{}})
                                     .first->value()
                                     .as_object();
            auto& snapshots_obj = instance_obj.insert(pair{"snapshots", boost::json::object{}})
                                      .first->value()
                                      .as_object();

            snapshots_obj.emplace(info.snapshot_info().fundamentals().snapshot_name(),
                                  std::move(snapshot_details));
        }
        else
        {
            assert(false && "either one of instance or snapshot details should be populated");
        }
    }

    return pretty_print({{"errors", boost::json::array{}}, {"info", std::move(info_obj)}});
}

std::string mp::JsonFormatter::format(const ListReply& reply) const
{
    boost::json::value output;

    if (reply.has_instance_list())
    {
        output = generate_instances_list(reply.instance_list());
    }
    else
    {
        assert(reply.has_snapshot_list() && "either one of the reports should be populated");
        output = generate_snapshots_list(reply.snapshot_list());
    }

    return pretty_print(output);
}

std::string mp::JsonFormatter::format(const NetworksReply& reply) const
{
    boost::json::array interfaces;
    for (const auto& interface : format::sorted(reply.interfaces()))
    {
        interfaces.push_back({{"name", interface.name()},
                              {"type", interface.type()},
                              {"description", interface.description()}});
    }

    return pretty_print({{"list", std::move(interfaces)}});
}

std::string mp::JsonFormatter::format(const FindReply& reply) const
{
    return pretty_print(
        {{"errors", boost::json::array{}}, {"images", format_images(reply.images_info())}});
}

std::string mp::JsonFormatter::format(const VersionReply& reply,
                                      const std::string& client_version) const
{
    boost::json::object version_json = {{"multipass", client_version}};
    if (!reply.version().empty())
    {
        version_json.emplace("multipassd", reply.version());

        if (mp::cmd::update_available(reply.update_info()))
        {
            version_json.emplace(
                "update",
                boost::json::object{{"title", reply.update_info().title()},
                                    {"description", reply.update_info().description()},
                                    {"url", reply.update_info().url()}});
        }
    }

    return pretty_print(version_json);
}

std::string mp::JsonFormatter::format(const mp::AliasDict& aliases) const
{
    return pretty_print(boost::json::value_from(aliases));
}

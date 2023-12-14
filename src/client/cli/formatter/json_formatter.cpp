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

#include <QJsonArray>
#include <QJsonObject>

namespace mp = multipass;

namespace
{
QJsonObject format_images(const google::protobuf::RepeatedPtrField<mp::FindReply_ImageInfo>& images_info)
{
    QJsonObject images_obj;

    for (const auto& image : images_info)
    {
        QJsonObject image_obj;
        image_obj.insert("os", QString::fromStdString(image.os()));
        image_obj.insert("release", QString::fromStdString(image.release()));
        image_obj.insert("version", QString::fromStdString(image.version()));

        QJsonArray aliases_arr;
        auto aliases = image.aliases_info();
        mp::format::filter_aliases(aliases);

        for (auto alias = aliases.cbegin() + 1; alias != aliases.cend(); alias++)
            aliases_arr.append(QString::fromStdString(alias->alias()));

        image_obj.insert("aliases", aliases_arr);
        image_obj.insert("remote", QString::fromStdString(aliases[0].remote_name()));

        images_obj.insert(QString::fromStdString(mp::format::image_string_for(aliases[0])), image_obj);
    }

    return images_obj;
}

QJsonObject generate_snapshot_details(const mp::DetailedInfoItem& item)
{
    const auto& snapshot_details = item.snapshot_info();
    const auto& fundamentals = snapshot_details.fundamentals();
    QJsonObject snapshot_info;

    snapshot_info.insert("size", QString::fromStdString(snapshot_details.size()));
    snapshot_info.insert("cpu_count", QString::fromStdString(item.cpu_count()));
    snapshot_info.insert("disk_space", QString::fromStdString(item.disk_total()));
    snapshot_info.insert("memory_size", QString::fromStdString(item.memory_total()));

    QJsonObject mounts;
    for (const auto& mount : item.mount_info().mount_paths())
    {
        QJsonObject entry;
        entry.insert("source_path", QString::fromStdString(mount.source_path()));

        mounts.insert(QString::fromStdString(mount.target_path()), entry);
    }
    snapshot_info.insert("mounts", mounts);

    snapshot_info.insert(
        "created",
        QString::fromStdString(MP_FORMAT_UTILS.convert_to_user_locale(fundamentals.creation_timestamp())));
    snapshot_info.insert("parent", QString::fromStdString(fundamentals.parent()));

    QJsonArray children;
    for (const auto& child : snapshot_details.children())
        children.append(QString::fromStdString(child));
    snapshot_info.insert("children", children);

    snapshot_info.insert("comment", QString::fromStdString(fundamentals.comment()));

    return snapshot_info;
}

QJsonObject generate_instance_details(const mp::DetailedInfoItem& item)
{
    const auto& instance_details = item.instance_info();

    QJsonObject instance_info;
    instance_info.insert("state", QString::fromStdString(mp::format::status_string_for(item.instance_status())));
    instance_info.insert("image_hash", QString::fromStdString(instance_details.id()));
    instance_info.insert("image_release", QString::fromStdString(instance_details.image_release()));
    instance_info.insert("release", QString::fromStdString(instance_details.current_release()));
    instance_info.insert("cpu_count", QString::fromStdString(item.cpu_count()));

    if (instance_details.has_num_snapshots())
        instance_info.insert("snapshot_count", QString::number(instance_details.num_snapshots()));

    QJsonArray load;
    if (!instance_details.load().empty())
    {
        auto loads = mp::utils::split(instance_details.load(), " ");
        for (const auto& entry : loads)
            load.append(std::stod(entry));
    }
    instance_info.insert("load", load);

    QJsonObject disks;
    QJsonObject disk;
    if (!instance_details.disk_usage().empty())
        disk.insert("used", QString::fromStdString(instance_details.disk_usage()));
    if (!item.disk_total().empty())
        disk.insert("total", QString::fromStdString(item.disk_total()));

    // TODO: disk name should come from daemon
    disks.insert("sda1", disk);
    instance_info.insert("disks", disks);

    QJsonObject memory;
    if (!instance_details.memory_usage().empty())
        memory.insert("used", std::stoll(instance_details.memory_usage()));
    if (!item.memory_total().empty())
        memory.insert("total", std::stoll(item.memory_total()));
    instance_info.insert("memory", memory);

    QJsonArray ipv4_addrs;
    for (const auto& ip : instance_details.ipv4())
        ipv4_addrs.append(QString::fromStdString(ip));
    instance_info.insert("ipv4", ipv4_addrs);

    QJsonObject mounts;
    for (const auto& mount : item.mount_info().mount_paths())
    {
        QJsonObject entry;
        QJsonArray mount_uids;
        QJsonArray mount_gids;

        auto mount_maps = mount.mount_maps();

        for (auto i = 0; i < mount_maps.uid_mappings_size(); ++i)
        {
            auto uid_map_pair = mount_maps.uid_mappings(i);
            auto host_uid = uid_map_pair.host_id();
            auto instance_uid = uid_map_pair.instance_id();

            mount_uids.append(QString("%1:%2")
                                  .arg(QString::number(host_uid))
                                  .arg((instance_uid == mp::default_id) ? "default" : QString::number(instance_uid)));
        }
        for (auto i = 0; i < mount_maps.gid_mappings_size(); ++i)
        {
            auto gid_map_pair = mount_maps.gid_mappings(i);
            auto host_gid = gid_map_pair.host_id();
            auto instance_gid = gid_map_pair.instance_id();

            mount_gids.append(QString("%1:%2")
                                  .arg(QString::number(host_gid))
                                  .arg((instance_gid == mp::default_id) ? "default" : QString::number(instance_gid)));
        }
        entry.insert("uid_mappings", mount_uids);
        entry.insert("gid_mappings", mount_gids);
        entry.insert("source_path", QString::fromStdString(mount.source_path()));

        mounts.insert(QString::fromStdString(mount.target_path()), entry);
    }
    instance_info.insert("mounts", mounts);

    return instance_info;
}

std::string generate_instances_list(const mp::InstancesList& instance_list)
{
    QJsonObject list_json;
    QJsonArray instances;

    for (const auto& instance : instance_list.instances())
    {
        QJsonObject instance_obj;
        instance_obj.insert("name", QString::fromStdString(instance.name()));
        instance_obj.insert("state", QString::fromStdString(mp::format::status_string_for(instance.instance_status())));

        QJsonArray ipv4_addrs;
        for (const auto& ip : instance.ipv4())
            ipv4_addrs.append(QString::fromStdString(ip));
        instance_obj.insert("ipv4", ipv4_addrs);

        instance_obj.insert("release",
                            QString::fromStdString(instance.current_release().empty()
                                                       ? "Not Available"
                                                       : fmt::format("Ubuntu {}", instance.current_release())));

        instances.append(instance_obj);
    }

    list_json.insert("list", instances);

    return MP_JSONUTILS.json_to_string(list_json);
}

std::string generate_snapshots_list(const mp::SnapshotsList& snapshot_list)
{
    QJsonObject info_json;
    QJsonObject info_obj;
    info_json.insert("errors", QJsonArray());

    for (const auto& item : snapshot_list.snapshots())
    {
        const auto& snapshot = item.fundamentals();
        QJsonObject snapshot_obj;

        snapshot_obj.insert("parent", QString::fromStdString(snapshot.parent()));
        snapshot_obj.insert("comment", QString::fromStdString(snapshot.comment()));

        const auto& it = info_obj.find(QString::fromStdString(item.name()));
        if (it == info_obj.end())
        {
            info_obj.insert(QString::fromStdString(item.name()),
                            QJsonObject{{QString::fromStdString(snapshot.snapshot_name()), snapshot_obj}});
        }
        else
        {
            QJsonObject obj = it.value().toObject();
            obj.insert(QString::fromStdString(snapshot.snapshot_name()), snapshot_obj);
            it.value() = obj;
        }
    }

    info_json.insert("info", info_obj);

    return MP_JSONUTILS.json_to_string(info_json);
}
} // namespace

std::string mp::JsonFormatter::format(const InfoReply& reply) const
{
    QJsonObject info_json;
    QJsonObject info_obj;

    info_json.insert("errors", QJsonArray());

    for (const auto& info : reply.details())
    {
        const auto& instance_it = info_obj.find(QString::fromStdString(info.name()));

        if (info.has_instance_info())
        {
            auto instance_details = generate_instance_details(info);

            // Nothing for the instance so far, so insert normally
            if (instance_it == info_obj.end())
            {
                info_obj.insert(QString::fromStdString(info.name()), instance_details);
            }
            // Some instance details already exist, so merge the values
            else
            {
                QJsonObject obj = instance_it.value().toObject();
                for (const auto& key : instance_details.keys())
                {
                    assert(obj.find(key) == obj.end() && "key already exists");
                    obj.insert(key, instance_details[key]);
                }
                instance_it.value() = obj;
            }
        }
        else
        {
            assert(info.has_snapshot_info() && "either one of instance or snapshot details should be populated");

            auto snapshot_details = generate_snapshot_details(info);

            // Nothing for the instance so far, so create the "snapshots" node and put snapshot details there
            if (instance_it == info_obj.end())
            {
                QJsonObject instance_obj, snapshot_obj;
                snapshot_obj.insert(QString::fromStdString(info.snapshot_info().fundamentals().snapshot_name()),
                                    snapshot_details);
                instance_obj.insert("snapshots", snapshot_obj);
                info_obj.insert(QString::fromStdString(info.name()), instance_obj);
            }
            // Some instance details already exist
            else
            {
                auto instance_obj = instance_it.value().toObject();
                auto snapshots_it = instance_obj.find("snapshots");
                QJsonObject snapshots_obj =
                    snapshots_it == instance_obj.end() ? QJsonObject() : snapshots_it.value().toObject();

                snapshots_obj.insert(QString::fromStdString(info.snapshot_info().fundamentals().snapshot_name()),
                                     snapshot_details);
                instance_obj.insert("snapshots", snapshots_obj);
                instance_it.value() = instance_obj;
            }
        }
    }
    info_json.insert("info", info_obj);

    return MP_JSONUTILS.json_to_string(info_json);
}

std::string mp::JsonFormatter::format(const ListReply& reply) const
{
    std::string output;

    if (reply.has_instance_list())
    {
        output = generate_instances_list(reply.instance_list());
    }
    else
    {
        assert(reply.has_snapshot_list() && "either one of the reports should be populated");
        output = generate_snapshots_list(reply.snapshot_list());
    }

    return output;
}

std::string mp::JsonFormatter::format(const NetworksReply& reply) const
{
    QJsonObject list_json;
    QJsonArray interfaces;

    for (const auto& interface : format::sorted(reply.interfaces()))
    {
        QJsonObject interface_obj;
        interface_obj.insert("name", QString::fromStdString(interface.name()));
        interface_obj.insert("type", QString::fromStdString(interface.type()));
        interface_obj.insert("description", QString::fromStdString(interface.description()));

        interfaces.append(interface_obj);
    }

    list_json.insert("list", interfaces);

    return MP_JSONUTILS.json_to_string(list_json);
}

std::string mp::JsonFormatter::format(const FindReply& reply) const
{
    QJsonObject find_json;

    find_json.insert("errors", QJsonArray());
    find_json.insert("blueprints", format_images(reply.blueprints_info()));
    find_json.insert("images", format_images(reply.images_info()));

    return MP_JSONUTILS.json_to_string(find_json);
}

std::string mp::JsonFormatter::format(const VersionReply& reply, const std::string& client_version) const
{
    QJsonObject version_json;

    version_json.insert("multipass", QString::fromStdString(client_version));

    if (!reply.version().empty())
    {
        version_json.insert("multipassd", QString::fromStdString(reply.version()));

        if (mp::cmd::update_available(reply.update_info()))
        {
            QJsonObject update;
            update.insert("title", QString::fromStdString(reply.update_info().title()));
            update.insert("description", QString::fromStdString(reply.update_info().description()));
            update.insert("url", QString::fromStdString(reply.update_info().url()));

            version_json.insert("update", update);
        }
    }
    return MP_JSONUTILS.json_to_string(version_json);
}

std::string mp::JsonFormatter::format(const mp::AliasDict& aliases) const
{
    return MP_JSONUTILS.json_to_string(aliases.to_json());
}

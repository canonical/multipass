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
#include <QJsonDocument>
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

std::string generate_instance_info_report(const mp::InfoReply& reply)
{
    QJsonObject info_json;
    QJsonObject info_obj;

    info_json.insert("errors", QJsonArray());

    for (const auto& info : reply.detailed_report().details())
    {
        const auto& instance_details = info.instance_info();

        QJsonObject instance_info;
        instance_info.insert("state", QString::fromStdString(mp::format::status_string_for(info.instance_status())));
        instance_info.insert("image_hash", QString::fromStdString(instance_details.id()));
        instance_info.insert("image_release", QString::fromStdString(instance_details.image_release()));
        instance_info.insert("release", QString::fromStdString(instance_details.current_release()));
        instance_info.insert("cpu_count", QString::fromStdString(info.cpu_count()));
        instance_info.insert("snapshots", QString::number(instance_details.num_snapshots()));

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
        if (!info.disk_total().empty())
            disk.insert("total", QString::fromStdString(info.disk_total()));

        // TODO: disk name should come from daemon
        disks.insert("sda1", disk);
        instance_info.insert("disks", disks);

        QJsonObject memory;
        if (!instance_details.memory_usage().empty())
            memory.insert("used", std::stoll(instance_details.memory_usage()));
        if (!info.memory_total().empty())
            memory.insert("total", std::stoll(info.memory_total()));
        instance_info.insert("memory", memory);

        QJsonArray ipv4_addrs;
        for (const auto& ip : instance_details.ipv4())
            ipv4_addrs.append(QString::fromStdString(ip));
        instance_info.insert("ipv4", ipv4_addrs);

        QJsonObject mounts;
        for (const auto& mount : info.mount_info().mount_paths())
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

                mount_uids.append(
                    QString("%1:%2")
                        .arg(QString::number(host_uid))
                        .arg((instance_uid == mp::default_id) ? "default" : QString::number(instance_uid)));
            }
            for (auto i = 0; i < mount_maps.gid_mappings_size(); ++i)
            {
                auto gid_map_pair = mount_maps.gid_mappings(i);
                auto host_gid = gid_map_pair.host_id();
                auto instance_gid = gid_map_pair.instance_id();

                mount_gids.append(
                    QString("%1:%2")
                        .arg(QString::number(host_gid))
                        .arg((instance_gid == mp::default_id) ? "default" : QString::number(instance_gid)));
            }
            entry.insert("uid_mappings", mount_uids);
            entry.insert("gid_mappings", mount_gids);
            entry.insert("source_path", QString::fromStdString(mount.source_path()));

            mounts.insert(QString::fromStdString(mount.target_path()), entry);
        }
        instance_info.insert("mounts", mounts);

        info_obj.insert(QString::fromStdString(info.name()), instance_info);
    }

    info_json.insert("info", info_obj);

    return mp::json_to_string(info_json);
}

std::string generate_snapshot_overview_report(const mp::InfoReply& reply)
{
    QJsonObject info_json;
    QJsonObject info_obj;

    info_json.insert("errors", QJsonArray());

    for (const auto& item : reply.snapshot_overview().overview())
    {
        const auto& snapshot = item.fundamentals();
        QJsonObject snapshot_obj;

        snapshot_obj.insert("parent", QString::fromStdString(snapshot.parent()));
        snapshot_obj.insert("comment", QString::fromStdString(snapshot.comment()));

        auto it = info_obj.find(QString::fromStdString(item.instance_name()));
        if (it == info_obj.end())
        {
            info_obj.insert(QString::fromStdString(item.instance_name()),
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

    return QString(QJsonDocument(info_json).toJson()).toStdString();
}
} // namespace

std::string mp::JsonFormatter::format(const InfoReply& reply) const
{
    std::string output;

    if (reply.has_detailed_report())
    {
        output = generate_instance_info_report(reply);
    }
    else
    {
        assert(reply.has_snapshot_overview() && "either one of the reports should be populated");
        output = generate_snapshot_overview_report(reply);
    }

    return output;
}

std::string mp::JsonFormatter::format(const ListReply& reply) const
{
    QJsonObject list_json;
    QJsonArray instances;

    for (const auto& instance : reply.instances())
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

    return mp::json_to_string(list_json);
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

    return mp::json_to_string(list_json);
}

std::string mp::JsonFormatter::format(const FindReply& reply) const
{
    QJsonObject find_json;

    find_json.insert("errors", QJsonArray());
    find_json.insert("blueprints", format_images(reply.blueprints_info()));
    find_json.insert("images", format_images(reply.images_info()));

    return mp::json_to_string(find_json);
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
    return mp::json_to_string(version_json);
}

std::string mp::JsonFormatter::format(const mp::AliasDict& aliases) const
{
    return mp::json_to_string(aliases.to_json());
}

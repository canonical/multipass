/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

#include <multipass/cli/json_formatter.h>

#include <multipass/cli/client_common.h>
#include <multipass/cli/format_utils.h>
#include <multipass/utils.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;

std::string mp::JsonFormatter::format(const InfoReply& reply) const
{
    QJsonObject info_json;
    QJsonObject info_obj;

    info_json.insert("errors", QJsonArray());

    for (const auto& info : reply.info())
    {
        QJsonObject instance_info;
        instance_info.insert("state", QString::fromStdString(mp::format::status_string_for(info.instance_status())));
        instance_info.insert("image_hash", QString::fromStdString(info.id()));
        instance_info.insert("image_release", QString::fromStdString(info.image_release()));
        instance_info.insert("release", QString::fromStdString(info.current_release()));

        QJsonArray load;
        if (!info.load().empty())
        {
            auto loads = mp::utils::split(info.load(), " ");
            for (const auto& entry : loads)
                load.append(std::stod(entry));
        }
        instance_info.insert("load", load);

        QJsonObject disks;
        QJsonObject disk;
        if (!info.disk_usage().empty())
            disk.insert("used", QString::fromStdString(info.disk_usage()));
        if (!info.disk_total().empty())
            disk.insert("total", QString::fromStdString(info.disk_total()));

        // TODO: disk name should come from daemon
        disks.insert("sda1", disk);
        instance_info.insert("disks", disks);

        QJsonObject memory;
        if (!info.memory_usage().empty())
            memory.insert("used", std::stoll(info.memory_usage()));
        if (!info.memory_total().empty())
            memory.insert("total", std::stoll(info.memory_total()));
        instance_info.insert("memory", memory);

        QJsonArray ipv4_addrs;
        for (const auto& ip : info.ipv4())
            ipv4_addrs.append(QString::fromStdString(ip));
        instance_info.insert("ipv4", ipv4_addrs);

        QJsonObject mounts;
        for (const auto& mount : info.mount_info().mount_paths())
        {
            QJsonObject entry;
            QJsonArray mount_uids;
            QJsonArray mount_gids;

            for (const auto& uid_map : mount.mount_maps().uid_map())
            {
                mount_uids.append(
                    QString("%1:%2")
                        .arg(QString::number(uid_map.first))
                        .arg((uid_map.second == mp::default_id) ? "default" : QString::number(uid_map.second)));
            }
            for (const auto& gid_map : mount.mount_maps().gid_map())
            {
                mount_gids.append(
                    QString("%1:%2")
                        .arg(QString::number(gid_map.first))
                        .arg((gid_map.second == mp::default_id) ? "default" : QString::number(gid_map.second)));
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

    return QString(QJsonDocument(info_json).toJson()).toStdString();
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

        instance_obj.insert("release", QString::fromStdString(instance.current_release()));

        instances.append(instance_obj);
    }

    list_json.insert("list", instances);

    return QString(QJsonDocument(list_json).toJson()).toStdString();
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

    return QString(QJsonDocument(list_json).toJson()).toStdString();
}

std::string mp::JsonFormatter::format(const FindReply& reply) const
{
    QJsonObject find_json;
    QJsonObject images;

    find_json.insert("errors", QJsonArray());

    for (const auto& image : reply.images_info())
    {
        QJsonObject image_obj;
        image_obj.insert("os", QString::fromStdString(image.os()));
        image_obj.insert("release", QString::fromStdString(image.release()));
        image_obj.insert("version", QString::fromStdString(image.version()));

        QJsonArray aliases_arr;
        auto aliases = image.aliases_info();
        mp::format::filter_aliases(aliases);

        for (auto alias = aliases.cbegin() + 1; alias != aliases.cend(); alias++)
        {
            aliases_arr.append(QString::fromStdString(alias->alias()));
        }
        image_obj.insert("aliases", aliases_arr);

        image_obj.insert("remote", QString::fromStdString(aliases[0].remote_name()));

        images.insert(QString::fromStdString(mp::format::image_string_for(aliases[0])), image_obj);
    }

    find_json.insert("images", images);

    return QString(QJsonDocument(find_json).toJson()).toStdString();
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
    return QString(QJsonDocument(version_json).toJson()).toStdString();
}

std::string mp::JsonFormatter::format(const mp::AliasDict& aliases) const
{
    QJsonObject aliases_json;
    QJsonArray aliases_array;

    for (const auto& elt : sort_dict(aliases))
    {
        const auto& name = elt.first;
        const auto& def = elt.second;

        QJsonObject alias_obj;
        alias_obj.insert("name", QString::fromStdString(name));
        alias_obj.insert("instance", QString::fromStdString(def.instance));
        alias_obj.insert("command", QString::fromStdString(def.command));

        aliases_array.append(alias_obj);
    }

    aliases_json.insert("aliases", aliases_array);

    return QString(QJsonDocument(aliases_json).toJson()).toStdString();
}

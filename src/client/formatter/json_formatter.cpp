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

#include <multipass/cli/json_formatter.h>

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
        if (!info.ipv4().empty())
            ipv4_addrs.append(QString::fromStdString(info.ipv4()));
        instance_info.insert("ipv4", ipv4_addrs);

        QJsonObject mounts;
        for (const auto& mount : info.mount_info().mount_paths())
        {
            QJsonObject entry;
            QJsonArray mount_uids;
            QJsonArray mount_gids;

            for (const auto uid_map : mount.mount_maps().uid_map())
            {
                mount_uids.append(
                    QString("%1:%2")
                        .arg(QString::number(uid_map.first))
                        .arg((uid_map.second == mp::default_id) ? "default" : QString::number(uid_map.second)));
            }
            for (const auto gid_map : mount.mount_maps().gid_map())
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
        if (!instance.ipv4().empty())
            ipv4_addrs.append(QString::fromStdString(instance.ipv4()));
        instance_obj.insert("ipv4", ipv4_addrs);

        instance_obj.insert("release", QString::fromStdString(instance.current_release()));

        instances.append(instance_obj);
    }

    list_json.insert("list", instances);

    return QString(QJsonDocument(list_json).toJson()).toStdString();
}

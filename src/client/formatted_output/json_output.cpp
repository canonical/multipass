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

#include <multipass/cli/format.h>
#include <multipass/cli/json_output.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;

std::string mp::JsonOutput::process_info(InfoReply& reply) const
{
    QJsonObject info_json;
    QJsonObject info_obj;

    info_json.insert("errors", QJsonArray());

    for (const auto& info : reply.info())
    {
        QJsonObject instance_info;
        instance_info.insert("state", QString::fromStdString(mp::format::status_string(info.instance_status())));
        instance_info.insert("image_hash", QString::fromStdString(info.id().substr(0, 12)));

        QJsonArray ipv4_addrs;
        if (!info.ipv4().empty())
            ipv4_addrs.append(QString::fromStdString(info.ipv4()));
        instance_info.insert("ipv4", ipv4_addrs);

        QJsonObject mounts;
        for (const auto& mount : info.mount_info().mount_paths())
        {
            QJsonObject entry;
            entry.insert("gid_mappings", QJsonArray());
            entry.insert("uid_mappings", QJsonArray());
            entry.insert("source_path", QString::fromStdString(mount.source_path()));

            mounts.insert(QString::fromStdString(mount.target_path()), entry);
        }
        instance_info.insert("mounts", mounts);

        info_obj.insert(QString::fromStdString(info.name()), instance_info);
    }
    info_json.insert("info", info_obj);

    return QString(QJsonDocument(info_json).toJson()).toStdString();
}

std::string mp::JsonOutput::process_list(ListReply& reply) const
{
    QJsonObject list_json;
    QJsonArray instances;

    for (const auto& instance : reply.instances())
    {
        QJsonObject instance_obj;
        instance_obj.insert("name", QString::fromStdString(instance.name()));
        instance_obj.insert("state", QString::fromStdString(mp::format::status_string(instance.instance_status())));

        QJsonArray ipv4_addrs;
        if (!instance.ipv4().empty())
            ipv4_addrs.append(QString::fromStdString(instance.ipv4()));
        instance_obj.insert("ipv4", ipv4_addrs);

        instances.append(instance_obj);
    }

    list_json.insert("list", instances);

    return QString(QJsonDocument(list_json).toJson()).toStdString();
}

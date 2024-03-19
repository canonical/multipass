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

#include <QJsonArray>

namespace mp = multipass;

namespace
{
mp::VMMount parse_json(const QJsonObject& json)
{
    mp::id_mappings uid_mappings;
    mp::id_mappings gid_mappings;
    auto source_path = json["source_path"].toString().toStdString();

    for (const QJsonValueRef uid_entry : json["uid_mappings"].toArray())
    {
        uid_mappings.push_back(
            {uid_entry.toObject()["host_uid"].toInt(), uid_entry.toObject()["instance_uid"].toInt()});
    }

    for (const QJsonValueRef gid_entry : json["gid_mappings"].toArray())
    {
        gid_mappings.push_back(
            {gid_entry.toObject()["host_gid"].toInt(), gid_entry.toObject()["instance_gid"].toInt()});
    }

    uid_mappings.erase(mp::unique_id_mappings(uid_mappings), uid_mappings.end());
    gid_mappings.erase(mp::unique_id_mappings(gid_mappings), gid_mappings.end());
    auto mount_type = mp::VMMount::MountType(json["mount_type"].toInt());

    return mp::VMMount{std::move(source_path), std::move(gid_mappings), std::move(uid_mappings), mount_type};
}
} // namespace

mp::VMMount::VMMount(const std::string& sourcePath,
                     id_mappings gidMappings,
                     id_mappings uidMappings,
                     MountType mountType)
    : source_path(sourcePath),
      gid_mappings(std::move(gidMappings)),
      uid_mappings(std::move(uidMappings)),
      mount_type(mountType)
{
    auto print_mappings = [](auto& iter_begin, const auto& iter_end) {
        std::string retval;
        while (iter_begin != iter_end)
        {
            retval += fmt::format("{}:{}; ", (*iter_begin).first, (*iter_begin).second);
            ++iter_begin;
        }
        return retval.substr(0, retval.size() - 2);
    };

    if (auto unique_iter_end = mp::unique_id_mappings(gid_mappings); unique_iter_end != gid_mappings.end())
        throw std::runtime_error(fmt::format("Mount cannot apply mapping with duplicate gids: {}",
                                             print_mappings(unique_iter_end, gid_mappings.end())));
    if (auto unique_iter_end = mp::unique_id_mappings(uid_mappings); unique_iter_end != uid_mappings.end())
        throw std::runtime_error(fmt::format("Mount cannot apply mapping with duplicate uids: {}",
                                             print_mappings(unique_iter_end, uid_mappings.end())));
}

mp::VMMount::VMMount(const QJsonObject& json) : VMMount{parse_json(json)} // delegate on copy ctor
{
}

QJsonObject mp::VMMount::serialize() const
{
    QJsonObject ret;
    ret.insert("source_path", QString::fromStdString(source_path));

    QJsonArray uid_mappings_json;

    for (const auto& map : uid_mappings)
    {
        QJsonObject map_entry;
        map_entry.insert("host_uid", map.first);
        map_entry.insert("instance_uid", map.second);

        uid_mappings_json.append(map_entry);
    }

    ret.insert("uid_mappings", uid_mappings_json);

    QJsonArray gid_mappings_json;

    for (const auto& map : gid_mappings)
    {
        QJsonObject map_entry;
        map_entry.insert("host_gid", map.first);
        map_entry.insert("instance_gid", map.second);

        gid_mappings_json.append(map_entry);
    }

    ret.insert("gid_mappings", gid_mappings_json);

    ret.insert("mount_type", static_cast<int>(mount_type));
    return ret;
}

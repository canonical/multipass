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

#include <multipass/utils.h>

#include "common.h"
#include "file_operations.h"
#include "json_test_utils.h"

#include <fmt/format.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace mpu = multipass::utils;

std::string make_instance_json(const std::optional<std::string>& default_mac,
                               const std::vector<mp::NetworkInterface>& extra_ifaces,
                               const std::vector<std::string>& extra_instances)
{
    QString contents("{\n"
                     "    \"real-zebraphant\": {\n"
                     "        \"deleted\": false,\n"
                     "        \"disk_space\": \"5368709120\",\n"
                     "        \"extra_interfaces\": [\n");

    QStringList extra_json;
    for (auto extra_interface : extra_ifaces)
    {
        extra_json += QString::fromStdString(fmt::format("            {{\n"
                                                         "                \"auto_mode\": {},\n"
                                                         "                \"id\": \"{}\",\n"
                                                         "                \"mac_address\": \"{}\"\n"
                                                         "            }}\n",
                                                         extra_interface.auto_mode, extra_interface.id,
                                                         extra_interface.mac_address));
    }
    contents += extra_json.join(',');

    contents += QString::fromStdString(fmt::format("        ],\n"
                                                   "        \"mac_addr\": \"{}\",\n"
                                                   "        \"mem_size\": \"1073741824\",\n"
                                                   "        \"metadata\": {{\n"
                                                   "            \"arguments\": [\n"
                                                   "                \"many\",\n"
                                                   "                \"arguments\"\n"
                                                   "            ],\n"
                                                   "            \"machine_type\": \"dmc-de-lorean\"\n"
                                                   "        }},\n"
                                                   "        \"mounts\": [\n"
                                                   "        ],\n"
                                                   "        \"num_cores\": 1,\n"
                                                   "        \"ssh_username\": \"ubuntu\",\n"
                                                   "        \"state\": 2\n"
                                                   "    }}\n",
                                                   default_mac ? *default_mac : mpu::generate_mac_address()));

    for (const auto& instance : extra_instances)
    {
        contents += QString::fromStdString(fmt::format(",\n"
                                                       "    \"{}\": {{\n"
                                                       "        \"deleted\": false,\n"
                                                       "        \"disk_space\": \"5368709120\",\n"
                                                       "        \"extra_interfaces\": [\n"
                                                       "        ],\n"
                                                       "        \"mac_addr\": \"{}\",\n"
                                                       "        \"mem_size\": \"1073741824\",\n"
                                                       "        \"metadata\": {{\n"
                                                       "            \"arguments\": [\n"
                                                       "                \"many\",\n"
                                                       "                \"arguments\"\n"
                                                       "            ],\n"
                                                       "            \"machine_type\": \"dmc-de-lorean\"\n"
                                                       "        }},\n"
                                                       "        \"mounts\": [\n"
                                                       "        ],\n"
                                                       "        \"num_cores\": 1,\n"
                                                       "        \"ssh_username\": \"ubuntu\",\n"
                                                       "        \"state\": 2\n"
                                                       "    }}",
                                                       instance, mpu::generate_mac_address()));
    }

    contents += "}";

    return contents.toStdString();
}

std::unique_ptr<mpt::TempDir> plant_instance_json(const std::string& contents)
{
    auto temp_dir = std::make_unique<mpt::TempDir>();
    QString filename(temp_dir->path() + "/multipassd-vm-instances.json");

    mpt::make_file_with_content(filename, contents);

    return temp_dir;
}

void check_interfaces_in_json(const QJsonObject& doc_object,
                              const std::string& mac,
                              const std::vector<mp::NetworkInterface>& extra_ifaces)
{
    const auto instance_object = doc_object["real-zebraphant"].toObject();
    const auto default_mac = instance_object["mac_addr"].toString().toStdString();
    ASSERT_EQ(default_mac, mac);

    const auto extra = instance_object["extra_interfaces"].toArray();
    ASSERT_EQ((unsigned)extra.size(), extra_ifaces.size());

    auto it = extra_ifaces.cbegin();
    for (const auto& extra_i : extra)
    {
        const auto interface = extra_i.toObject();
        ASSERT_EQ(interface["mac_address"].toString().toStdString(), it->mac_address);
        ASSERT_EQ(interface["id"].toString().toStdString(), it->id);
        ASSERT_EQ(interface["auto_mode"].toBool(), it->auto_mode);
        ++it;
    }
}

void check_interfaces_in_json(const QString& file,
                              const std::string& mac,
                              const std::vector<mp::NetworkInterface>& extra_ifaces)
{
    QByteArray json = mpt::load(file);

    QJsonParseError parse_error;
    const auto doc = QJsonDocument::fromJson(json, &parse_error);
    EXPECT_FALSE(doc.isNull());
    EXPECT_TRUE(doc.isObject());

    check_interfaces_in_json(doc.object(), mac, extra_ifaces);
}

void check_mounts_in_json(const QJsonObject& doc_object, const std::unordered_map<std::string, mp::VMMount>& mounts)
{
    const auto instance_object = doc_object["real-zebraphant"].toObject();
    const auto json_mounts = instance_object["mounts"].toArray();
    ASSERT_EQ(json_mounts.count(), mounts.size());

    for (const QJsonValue& json_mount : json_mounts)
    {
        const auto& json_target_path = json_mount["target_path"].toString().toStdString();
        const auto& json_source_path = json_mount["source_path"].toString().toStdString();
        const auto& json_uid_mapping = json_mount["uid_mappings"].toArray();
        const auto& json_gid_mapping = json_mount["gid_mappings"].toArray();

        ASSERT_EQ(mounts.count(json_target_path), 1);
        const auto& original_mount = mounts.at(json_target_path);

        ASSERT_EQ(original_mount.get_source_path(), json_source_path);

        ASSERT_EQ(json_uid_mapping.count(), original_mount.get_uid_mappings().size());
        for (auto i = 0; i < json_uid_mapping.count(); ++i)
        {
            ASSERT_EQ(json_uid_mapping[i]["host_uid"], original_mount.get_uid_mappings()[i].first);
            ASSERT_EQ(json_uid_mapping[i]["instance_uid"], original_mount.get_uid_mappings()[i].second);
        }

        ASSERT_EQ(json_gid_mapping.count(), original_mount.get_gid_mappings().size());
        for (auto i = 0; i < json_gid_mapping.count(); ++i)
        {
            ASSERT_EQ(json_gid_mapping[i]["host_gid"], original_mount.get_gid_mappings()[i].first);
            ASSERT_EQ(json_gid_mapping[i]["instance_gid"], original_mount.get_gid_mappings()[i].second);
        }
    }
}

void check_mounts_in_json(const QString& file, const std::unordered_map<std::string, mp::VMMount>& mounts)
{
    QByteArray json = mpt::load(file);

    QJsonParseError parse_error;
    const auto doc = QJsonDocument::fromJson(json, &parse_error);
    EXPECT_FALSE(doc.isNull());
    EXPECT_TRUE(doc.isObject());

    check_mounts_in_json(doc.object(), mounts);
}

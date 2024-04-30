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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <stdexcept>

namespace mp = multipass;
namespace mpu = multipass::utils;

mp::JsonUtils::JsonUtils(const Singleton<JsonUtils>::PrivatePass& pass) noexcept : Singleton<JsonUtils>{pass}
{
}

void mp::JsonUtils::write_json(const QJsonObject& root, QString file_name) const
{
    auto dir = QFileInfo(file_name).absoluteDir();
    if (!MP_FILEOPS.mkpath(dir, "."))
        throw std::runtime_error(fmt::format("Could not create path '{}'", dir.absolutePath()));

    QSaveFile db_file{file_name};
    if (!MP_FILEOPS.open(db_file, QIODevice::WriteOnly))
        throw std::runtime_error{fmt::format("Could not open transactional file for writing; filename: {}", file_name)};

    if (MP_FILEOPS.write(db_file, QJsonDocument{root}.toJson()) == -1)
        throw std::runtime_error{fmt::format("Could not write json to transactional file; filename: {}; error: {}",
                                             file_name,
                                             db_file.errorString())};

    if (!MP_FILEOPS.commit(db_file))
        throw std::runtime_error{fmt::format("Could not commit transactional file; filename: {}", file_name)};
}

std::string mp::JsonUtils::json_to_string(const QJsonObject& root) const
{
    // The function name toJson() is shockingly wrong, for it converts an actual JsonDocument to a QByteArray.
    return QJsonDocument(root).toJson().toStdString();
}

QJsonValue mp::JsonUtils::update_cloud_init_instance_id(const QJsonValue& cloud_init_instance_id_value,
                                                        const std::string& src_vm_name,
                                                        const std::string& dest_vm_name) const
{
    QJsonValue result_cloud_init_instance_id = cloud_init_instance_id_value;
    std::string cloud_init_instance_id_str = result_cloud_init_instance_id.toString().toStdString();

    assert(cloud_init_instance_id_str.size() >= src_vm_name.size());
    result_cloud_init_instance_id =
        QString::fromStdString(cloud_init_instance_id_str.replace(0, src_vm_name.size(), dest_vm_name));

    return result_cloud_init_instance_id;
}

QJsonValue mp::JsonUtils::update_unique_identifiers_of_metadata(const QJsonValue& metadata_value,
                                                                const multipass::VMSpecs& src_specs,
                                                                const multipass::VMSpecs& dest_specs,
                                                                const std::string& src_vm_name,
                                                                const std::string& dest_vm_name) const
{
    assert(src_specs.extra_interfaces.size() == dest_specs.extra_interfaces.size());

    QJsonObject result_metadata_object = metadata_value.toObject();
    QJsonValueRef arguments = result_metadata_object["arguments"];
    QJsonArray json_array = arguments.toArray();
    for (QJsonValueRef item : json_array)
    {
        QString str = item.toString();

        str.replace(src_specs.default_mac_address.c_str(), dest_specs.default_mac_address.c_str());
        for (size_t i = 0; i < src_specs.extra_interfaces.size(); ++i)
        {
            const std::string& src_extra_interface_mac_addr = src_specs.extra_interfaces[i].mac_address;
            if (!src_extra_interface_mac_addr.empty())
            {
                const std::string& dest_extra_interface_mac_addr = dest_specs.extra_interfaces[i].mac_address;
                str.replace(src_extra_interface_mac_addr.c_str(), dest_extra_interface_mac_addr.c_str());
            }
        }
        // string replacement is "instances/<src_name>"->"instances/<dest_name>" instead of
        // "<src_name>"->"<dest_name>" is because the second one might match other substrings of the metadata.
        str.replace("instances/" + QString{src_vm_name.c_str()}, "instances/" + QString{dest_vm_name.c_str()});
        item = str;
    }
    arguments = json_array;

    return QJsonValue{result_metadata_object};
}

QJsonArray mp::JsonUtils::extra_interfaces_to_json_array(
    const std::vector<mp::NetworkInterface>& extra_interfaces) const
{
    QJsonArray json;

    for (const auto& interface : extra_interfaces)
    {
        QJsonObject entry;
        entry.insert("id", QString::fromStdString(interface.id));
        entry.insert("mac_address", QString::fromStdString(interface.mac_address));
        entry.insert("auto_mode", interface.auto_mode);
        json.append(entry);
    }

    return json;
}

std::optional<std::vector<mp::NetworkInterface>> mp::JsonUtils::read_extra_interfaces(const QJsonObject& record) const
{
    if (record.contains("extra_interfaces"))
    {
        std::vector<mp::NetworkInterface> extra_interfaces;

        for (QJsonValueRef entry : record["extra_interfaces"].toArray())
        {
            auto id = entry.toObject()["id"].toString().toStdString();
            auto mac_address = entry.toObject()["mac_address"].toString().toStdString();
            if (!mpu::valid_mac_address(mac_address))
            {
                throw std::runtime_error(fmt::format("Invalid MAC address {}", mac_address));
            }
            auto auto_mode = entry.toObject()["auto_mode"].toBool();
            extra_interfaces.push_back(mp::NetworkInterface{id, mac_address, auto_mode});
        }

        return extra_interfaces;
    }

    return std::nullopt;
}

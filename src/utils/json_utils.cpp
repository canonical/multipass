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

#include <multipass/format.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

#include <QJsonArray>
#include <QJsonDocument>

#include <stdexcept>

namespace mp = multipass;
namespace mpu = multipass::utils;

mp::JsonUtils::JsonUtils(const Singleton<JsonUtils>::PrivatePass& pass) noexcept
    : Singleton<JsonUtils>{pass}
{
}

std::string mp::JsonUtils::json_to_string(const QJsonObject& root) const
{
    // The function name toJson() is shockingly wrong, for it converts an actual JsonDocument to a
    // QByteArray.
    return QJsonDocument(root).toJson().toStdString();
}

QJsonValue mp::JsonUtils::update_cloud_init_instance_id(const QJsonValue& id,
                                                        const std::string& src_vm_name,
                                                        const std::string& dest_vm_name) const
{
    std::string id_str = id.toString().toStdString();
    assert(id_str.size() >= src_vm_name.size());

    return QJsonValue{QString::fromStdString(id_str.replace(0, src_vm_name.size(), dest_vm_name))};
}

QJsonValue mp::JsonUtils::update_unique_identifiers_of_metadata(
    const QJsonValue& metadata,
    const multipass::VMSpecs& src_specs,
    const multipass::VMSpecs& dest_specs,
    const std::string& src_vm_name,
    const std::string& dest_vm_name) const
{
    assert(src_specs.extra_interfaces.size() == dest_specs.extra_interfaces.size());

    QJsonObject result_metadata_object = metadata.toObject();
    QJsonValueRef arguments = result_metadata_object["arguments"];
    QJsonArray json_array = arguments.toArray();
    for (QJsonValueRef item : json_array)
    {
        QString str = item.toString();

        str.replace(src_specs.default_mac_address.c_str(), dest_specs.default_mac_address.c_str());
        for (size_t i = 0; i < src_specs.extra_interfaces.size(); ++i)
        {
            const std::string& src_mac = src_specs.extra_interfaces[i].mac_address;
            if (!src_mac.empty())
            {
                const std::string& dest_mac = dest_specs.extra_interfaces[i].mac_address;
                str.replace(src_mac.c_str(), dest_mac.c_str());
            }
        }
        // string replacement is "instances/<src_name>"->"instances/<dest_name>" instead of
        // "<src_name>"->"<dest_name>", because the second one might match other substrings of the
        // metadata.
        str.replace("instances/" + QString{src_vm_name.c_str()},
                    "instances/" + QString{dest_vm_name.c_str()});
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

std::optional<std::vector<mp::NetworkInterface>> mp::JsonUtils::read_extra_interfaces(
    const QJsonObject& record) const
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

boost::json::value mp::qjson_to_boost_json(const QJsonValue& value)
{
    QJsonDocument doc;
    switch (value.type())
    {
    case QJsonValue::Array:
        doc = QJsonDocument{value.toArray()};
        break;
    case QJsonValue::Object:
        doc = QJsonDocument{value.toObject()};
        break;
    default:
        assert(false && "unsupported type");
    }
    return boost::json::parse(std::string_view(doc.toJson()));
}

QJsonValue mp::boost_json_to_qjson(const boost::json::value& value)
{
    auto json_data = serialize(value);
    auto doc = QJsonDocument::fromJson(
        QByteArray{json_data.data(), static_cast<qsizetype>(json_data.size())});
    if (doc.isArray())
        return doc.array();
    else if (doc.isObject())
        return doc.object();

    assert(false && "unsupported type");
    std::abort();
}

void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const QStringList& list)
{
    auto& arr = json.emplace_array();
    for (const auto& i : list)
        arr.emplace_back(i.toStdString());
}

QStringList tag_invoke(const boost::json::value_to_tag<QStringList>&,
                       const boost::json::value& json)
{
    QStringList result;
    for (const auto& i : json.as_array())
        result.emplace_back(value_to<QString>(i));
    return result;
}

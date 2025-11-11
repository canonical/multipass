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

#pragma once

#include "singleton.h"

#include <multipass/network_interface.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <vector>

#define MP_JSONUTILS multipass::JsonUtils::instance()

namespace multipass
{
struct VMSpecs;
class JsonUtils : public Singleton<JsonUtils>
{
public:
    explicit JsonUtils(const Singleton<JsonUtils>::PrivatePass&) noexcept;

    virtual std::string json_to_string(const QJsonObject& root) const;
    virtual QJsonValue update_cloud_init_instance_id(const QJsonValue& id,
                                                     const std::string& src_vm_name,
                                                     const std::string& dest_vm_name) const;
    virtual QJsonValue update_unique_identifiers_of_metadata(const QJsonValue& metadata,
                                                             const multipass::VMSpecs& src_specs,
                                                             const multipass::VMSpecs& dest_specs,
                                                             const std::string& src_vm_name,
                                                             const std::string& dest_vm_name) const;
    virtual QJsonArray extra_interfaces_to_json_array(
        const std::vector<NetworkInterface>& extra_interfaces) const;
    virtual std::optional<std::vector<NetworkInterface>> read_extra_interfaces(
        const QJsonObject& record) const;
};

// (De)serialize mappings to/from JSON arrays by setting the map key as a JSON field in each
// element.
struct MapAsJsonArray
{
    std::string key_field;
};

template <typename T>
    requires boost::json::is_map_like<T>::value
void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const T& mapping,
                const MapAsJsonArray& cfg)
{
    auto& arr = json.emplace_array();
    for (const auto& [key, value] : mapping)
    {
        auto elem = boost::json::value_from(value);
        elem.as_object().emplace(cfg.key_field, boost::json::value_from(key));
        arr.push_back(std::move(elem));
    }
}

template <typename T>
    requires boost::json::is_map_like<T>::value
T tag_invoke(const boost::json::value_to_tag<T>&,
             const boost::json::value& json,
             const MapAsJsonArray& cfg)
{
    T result;
    for (const auto& i : json.as_array())
    {
        boost::json::value elem = i;
        auto key = value_to<typename T::key_type>(elem.at(cfg.key_field));
        elem.as_object().erase(cfg.key_field);
        result.emplace(key, value_to<typename T::mapped_type>(elem));
    }
    return result;
}

// Temporary conversion functions to migrate between Qt and Boost JSON values.
boost::json::value qjson_to_boost_json(const QJsonValue& value);
QJsonValue boost_json_to_qjson(const boost::json::value& value);
} // namespace multipass

// These are in the global namespace so that Boost.JSON can look them up via ADL for `QString` and
// `QStringList`.
inline void tag_invoke(const boost::json::value_from_tag&,
                       boost::json::value& json,
                       const QString& string)
{
    json = string.toStdString();
}

inline QString tag_invoke(const boost::json::value_to_tag<QString>&, const boost::json::value& json)
{
    return QString::fromStdString(value_to<std::string>(json));
}

void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const QStringList& list);

QStringList tag_invoke(const boost::json::value_to_tag<QStringList>&,
                       const boost::json::value& json);

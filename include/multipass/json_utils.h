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
#include "utils/sorted_map_view.h"

#include <multipass/network_interface.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <boost/json.hpp>

#include <filesystem>
#include <optional>
#include <ostream>
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

    virtual QJsonObject read_object_from_file(const std::filesystem::path& file_path) const;
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

namespace detail
{
inline auto if_contains(const boost::json::value& json, std::size_t key)
{
    return json.as_array().if_contains(key);
}

inline auto if_contains(const boost::json::value& json, std::string_view key)
{
    return json.as_object().if_contains(key);
}
} // namespace detail

template <typename T, typename Key, typename U = T, typename... Context>
    requires(sizeof...(Context) <= 1)
T lookup_or(const boost::json::value& json, Key&& key, U&& fallback, const Context&... ctx)
{
    if (auto elem = detail::if_contains(json, std::forward<Key>(key)))
        return value_to<T>(*elem, ctx...);
    else
        return std::forward<U>(fallback);
}

// Prevent implicit conversions to `boost::json::value`.
template <typename T, typename Value, typename Key, typename U = T, typename... Context>
T lookup_or(const Value&, Key&&, U&&, const Context&...) = delete;

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

struct SortJsonKeys
{
};

template <typename T>
    requires boost::json::is_map_like<T>::value
void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const T& mapping,
                const SortJsonKeys&)
{
    auto& obj = json.emplace_object();
    for (const auto& [key, value] : sorted_map_view(mapping))
        obj.emplace(key.get(), boost::json::value_from(value.get()));
}

struct PrettyPrintOptions
{
    int indent = 4;
    bool trailing_newline = true;
};

void pretty_print(std::ostream& os,
                  const boost::json::value& value,
                  const PrettyPrintOptions& opts = {});
std::string pretty_print(const boost::json::value& value, const PrettyPrintOptions& opts = {});

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

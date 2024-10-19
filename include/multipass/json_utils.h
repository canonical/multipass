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

#ifndef MULTIPASS_JSON_UTILS_H
#define MULTIPASS_JSON_UTILS_H

#include "singleton.h"

#include <multipass/network_interface.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

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

    virtual void write_json(const QJsonObject& root, QString file_name) const; // transactional; creates parent dirs
    virtual std::string json_to_string(const QJsonObject& root) const;
    virtual QJsonValue update_cloud_init_instance_id(const QJsonValue& cloud_init_instance_id_value,
                                                     const std::string& src_vm_name,
                                                     const std::string& dest_vm_name) const;
    virtual QJsonValue update_unique_identifiers_of_metadata(const QJsonValue& metadata_value,
                                                             const multipass::VMSpecs& src_specs,
                                                             const multipass::VMSpecs& dest_specs,
                                                             const std::string& src_vm_name,
                                                             const std::string& dest_vm_name) const;
    virtual QJsonArray extra_interfaces_to_json_array(const std::vector<NetworkInterface>& extra_interfaces) const;
    virtual std::optional<std::vector<NetworkInterface>> read_extra_interfaces(const QJsonObject& record) const;
};
} // namespace multipass
#endif // MULTIPASS_JSON_UTILS_H

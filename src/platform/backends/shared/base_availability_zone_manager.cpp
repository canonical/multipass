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

#include <multipass/base_availability_zone.h>
#include <multipass/base_availability_zone_manager.h>
#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <QJsonDocument>

namespace
{
constexpr auto category = "az-manager";
constexpr auto az_file = "az-manager.json";
constexpr auto zones_directory_name = "zones";
constexpr auto automatic_zone_key = "automatic_zone";
} // namespace

namespace mpl = multipass::logging;

namespace multipass
{
auto create_default_zones(const fs::path& zones_directory)
{
    std::array<AvailabilityZone::UPtr, default_zone_names.size()> zones{};
    std::transform(default_zone_names.begin(), default_zone_names.end(), zones.begin(), [&](const auto& zone_name) {
        return std::make_unique<BaseAvailabilityZone>(zone_name, zones_directory);
    });
    return zones;
};

BaseAvailabilityZoneManager::data BaseAvailabilityZoneManager::read_from_file(
    const std::filesystem::path& file_path,
    const std::filesystem::path& zones_directory)
{
    mpl::debug(category, "reading AZ manager from file '{}'", file_path);

    const auto read_json = [&] {
        try
        {
            return MP_JSONUTILS.read_object_from_file(file_path);
        }
        catch (std::exception& e)
        {
            mpl::warn(category, "failed to read AZ manager file': {}", e.what());
            return QJsonObject{};
        }
    };

    const auto json = read_json();

    const auto deserialize_automatic_zone = [&] {
        const auto json_automatic_zone = json[automatic_zone_key].toString().toStdString();
        if (std::find(default_zone_names.begin(), default_zone_names.end(), json_automatic_zone) !=
            default_zone_names.end())
            return json_automatic_zone;

        mpl::debug(category, "automatic zone '{}' not known, using default", json_automatic_zone);
        return std::string{*default_zone_names.begin()};
    };

    return {
        // TODO remove these comments in C++20
        /*.file_path = */ file_path,
        /*.zone_collection = */ {create_default_zones(zones_directory), deserialize_automatic_zone()},
    };
}

BaseAvailabilityZoneManager::BaseAvailabilityZoneManager(const fs::path& data_dir)
    : m{read_from_file(data_dir / az_file, data_dir / zones_directory_name)}
{
    serialize();
}

AvailabilityZone& BaseAvailabilityZoneManager::get_zone(const std::string& name)
{
    for (const auto& zone : m.zone_collection.zones)
    {
        if (zone->get_name() == name)
            return *zone;
    }
    throw AvailabilityZoneNotFound{name};
}

std::string BaseAvailabilityZoneManager::get_automatic_zone_name()
{
    const auto zone_name = m.zone_collection.next_available();
    serialize();
    return zone_name;
}

std::vector<std::reference_wrapper<const AvailabilityZone>> BaseAvailabilityZoneManager::get_zones()
{
    std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
    zone_list.reserve(m.zone_collection.zones.size());
    for (auto& zone : m.zone_collection.zones)
        zone_list.emplace_back(*zone);
    return zone_list;
}

std::string BaseAvailabilityZoneManager::get_default_zone_name() const
{
    return m.zone_collection.zones.begin()->get()->get_name();
}

void BaseAvailabilityZoneManager::serialize() const
{
    mpl::debug(category, "writing AZ manager to file '{}'", m.file_path);
    const std::unique_lock lock{m.mutex};

    const QJsonObject json{
        {automatic_zone_key, QString::fromStdString(m.zone_collection.last_used())},
    };

    MP_JSONUTILS.write_json(json, QString::fromStdString(m.file_path.u8string()));
}

BaseAvailabilityZoneManager::ZoneCollection::ZoneCollection(
    std::array<AvailabilityZone::UPtr, default_zone_names.size()>&& _zones,
    std::string last_used)
    : zones{std::move(_zones)}, automatic_zone{std::find_if(zones.begin(), zones.end(), [&last_used](const auto& zone) {
          return zone->get_name() == last_used;
      })}
{
}

std::string BaseAvailabilityZoneManager::ZoneCollection::next_available()
{
    const auto start = automatic_zone;
    auto current = start;
    do
    {
        if (current->get()->is_available())
        {
            const auto result = current->get()->get_name();
            if (++current == zones.end())
                current = zones.begin();
            automatic_zone = current;
            return result;
        }

        if (++current == zones.end())
            current = zones.begin();
    } while (current != start);

    throw NoAvailabilityZoneAvailable{};
}

std::string BaseAvailabilityZoneManager::ZoneCollection::last_used() const
{
    return automatic_zone->get()->get_name();
}

} // namespace multipass

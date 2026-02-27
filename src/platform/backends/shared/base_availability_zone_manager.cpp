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

namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "az-manager";
constexpr auto az_file = "az-manager.json";
constexpr auto zones_directory_name = "zones";
constexpr auto automatic_zone_key = "automatic_zone";

[[nodiscard]] auto create_default_zones(const multipass::fs::path& zones_directory)
{
    using namespace multipass;

    std::array<AvailabilityZone::UPtr, default_zone_names.size()> zones{};
    size_t idx = 0;
    for (const auto& zone_name : default_zone_names)
        zones[idx++] = std::make_unique<BaseAvailabilityZone>(zone_name, zones_directory);

    return zones;
};
} // namespace

namespace multipass
{

BaseAvailabilityZoneManager::BaseAvailabilityZoneManager(const fs::path& data_dir)
    : file_path{data_dir / az_file},
      zone_collection{create_default_zones(data_dir / zones_directory_name), load_file(file_path)}
{
    save_file();
}

AvailabilityZone& BaseAvailabilityZoneManager::get_zone(const std::string& name)
{
    for (const auto& zone : zones())
    {
        if (zone->get_name() == name)
            return *zone;
    }
    throw AvailabilityZoneNotFound{name};
}

std::string BaseAvailabilityZoneManager::get_automatic_zone_name()
{
    const auto zone_name = zone_collection.next_available();
    save_file();
    return zone_name;
}

std::vector<std::reference_wrapper<const AvailabilityZone>> BaseAvailabilityZoneManager::get_zones()
{
    std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
    zone_list.reserve(zones().size());
    for (auto& zone : zones())
        zone_list.emplace_back(*zone);
    return zone_list;
}

std::string BaseAvailabilityZoneManager::get_default_zone_name() const
{
    return (*zones().begin())->get_name();
}

std::string BaseAvailabilityZoneManager::load_file(const std::filesystem::path& file_path)
{
    mpl::debug(category, "reading AZ manager from file '{}'", file_path);
    if (auto filedata = MP_FILEOPS.try_read_file(file_path))
    {
        try
        {
            auto json = boost::json::parse(*filedata);
            return value_to<std::string>(json.at(automatic_zone_key));
        }
        catch (const boost::system::system_error& e)
        {
            mpl::error(category, "Error parsing file '{}': {}", file_path, e.what());
        }
    }
    // Return a default value if we couldn't load from `file_path`.
    return "";
}

void BaseAvailabilityZoneManager::save_file() const
{
    mpl::debug(category, "writing AZ manager to file '{}'", file_path);
    const std::unique_lock lock{mutex};

    boost::json::value json = {{automatic_zone_key, zone_collection.last_used()}};
    MP_FILEOPS.write_transactionally(QString::fromStdString(file_path.string()),
                                     pretty_print(json));
}

const BaseAvailabilityZoneManager::ZoneCollection::ZoneArray&
BaseAvailabilityZoneManager::zones() const
{
    return zone_collection.zones;
}

BaseAvailabilityZoneManager::ZoneCollection::ZoneCollection(
    std::array<AvailabilityZone::UPtr, default_zone_names.size()>&& _zones,
    std::string last_used)
    : zones{std::move(_zones)},
      automatic_zone{std::find_if(zones.begin(), zones.end(), [&last_used](const auto& zone) {
          return zone->get_name() == last_used;
      })}
{
    if (automatic_zone == zones.end())
    {
        mpl::debug(category, "automatic zone '{}' not known, using default", last_used);
        automatic_zone = zones.begin();
    }
}

std::string BaseAvailabilityZoneManager::ZoneCollection::next_available()
{
    std::unique_lock lock{mutex};

    // Locate the first available zone
    auto zone_it = std::find_if(zones.begin(), zones.end(), [](const auto& zone) {
        return zone->is_available();
    });

    // Check if an available zone was found
    if (zone_it != zones.end())
    {
        automatic_zone = zone_it;
        return (*zone_it)->get_name();
    }

    // If none are available, throw an exception
    throw NoAvailabilityZoneAvailable{};
}

std::string BaseAvailabilityZoneManager::ZoneCollection::last_used() const
{
    std::shared_lock lock{mutex};
    return automatic_zone->get()->get_name();
}

} // namespace multipass
